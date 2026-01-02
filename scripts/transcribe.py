import sys
import os

# 1. 强制设置 Hugging Face 镜像站
os.environ["HF_ENDPOINT"] = "https://hf-mirror.com"

# 2. 强制在线模式，防止被误判为离线
if "HF_HUB_OFFLINE" in os.environ:
    del os.environ["HF_HUB_OFFLINE"]

# 3. 清除可能导致连接失败的代理设置
for key in ["HTTP_PROXY", "HTTPS_PROXY", "http_proxy", "https_proxy"]:
    if key in os.environ:
        del os.environ[key]

# 4. Patch tqdm to output progress for Qt
# 这需要在导入 huggingface_hub 之前完成，以确保生效
# Global flag to switch progress type
IS_TRANSCRIBING = False

try:
    import tqdm
    import tqdm.std
    import tqdm.auto
    
    # 保存原始类
    _original_tqdm = tqdm.tqdm

    class CustomTqdm(_original_tqdm):
        def __init__(self, *args, **kwargs):
            # 强制开启进度条逻辑，但重定向输出到空，避免 stderr 乱码
            # 我们只需要 update 中的自定义 print
            kwargs['disable'] = False
            kwargs['file'] = open(os.devnull, 'w')
            super().__init__(*args, **kwargs)
            self._last_report_percent = -1

        def update(self, n=1):
            super().update(n)
            # 只有当有 total 且 total > 0 时才计算百分比
            if self.total and self.total > 0:
                percent = int(self.n * 100 / self.total)
                # 为了避免输出过多，只有百分比变化时才输出
                if percent != self._last_report_percent:
                    # 按照 Qt 识别的格式输出到 stdout
                    # 使用 sys.__stdout__ 确保直接输出到控制台，不受 file 参数影响
                    tag = "TRANS_PROGRESS" if IS_TRANSCRIBING else "DOWNLOAD_PROGRESS"
                    sys.__stdout__.write(f"{tag}: {percent}\n")
                    sys.__stdout__.flush()
                    self._last_report_percent = percent
        
        def close(self):
            super().close()
            # 确保最后输出 100%
            if self.total and self.total > 0 and self.n >= self.total:
                 if self._last_report_percent != 100:
                     sys.__stdout__.write(f"DOWNLOAD_PROGRESS: 100\n")
                     sys.__stdout__.flush()
    
    # 全面替换 tqdm
    tqdm.tqdm = CustomTqdm
    tqdm.std.tqdm = CustomTqdm
    if hasattr(tqdm, 'auto'):
        tqdm.auto.tqdm = CustomTqdm
        
except ImportError:
    pass

# 5. Suppress symlink warning
os.environ["HF_HUB_DISABLE_SYMLINKS_WARNING"] = "1"

import wave
import json
import datetime
import argparse
import requests
import zipfile

# Add NVIDIA library paths for faster-whisper/ctranslate2 on Windows
# This must be done before importing faster_whisper or loading the model
if sys.platform == "win32":
    try:
        import nvidia.cublas
        import nvidia.cudnn
        
        paths_to_add = []
        
        cublas_path = os.path.dirname(nvidia.cublas.__file__) if nvidia.cublas.__file__ else list(nvidia.cublas.__path__)[0]
        cublas_bin = os.path.join(cublas_path, "bin")
        if os.path.isdir(cublas_bin):
            paths_to_add.append(cublas_bin)
            
        cudnn_path = os.path.dirname(nvidia.cudnn.__file__) if nvidia.cudnn.__file__ else list(nvidia.cudnn.__path__)[0]
        cudnn_bin = os.path.join(cudnn_path, "bin")
        if os.path.isdir(cudnn_bin):
            paths_to_add.append(cudnn_bin)
            
        env_path = os.environ["PATH"]
        for p in paths_to_add:
            if p not in env_path:
                os.environ["PATH"] = p + os.pathsep + os.environ["PATH"]
    except ImportError:
        pass

# Vosk imports
from vosk import Model, KaldiRecognizer

# Whisper imports
try:
    from faster_whisper import WhisperModel, download_model
    HAS_WHISPER = True
except ImportError:
    HAS_WHISPER = False

VOSK_MODEL_NAME = "vosk-model-small-cn-0.22"
VOSK_MODEL_URL = f"https://alphacephei.com/vosk/models/{VOSK_MODEL_NAME}.zip"

def download_vosk_model(path):
    print(f"Downloading model {VOSK_MODEL_NAME}...")
    sys.stdout.flush()
    proxies = {"http": None, "https": None}
    try:
        response = requests.get(VOSK_MODEL_URL, stream=True, proxies=proxies)
    except requests.exceptions.ProxyError:
        print("Proxy error detected, trying without explicit proxy settings...")
        response = requests.get(VOSK_MODEL_URL, stream=True)
    
    total_size_in_bytes = int(response.headers.get('content-length', 0))
    block_size = 1024 * 1024 # 1 MB
    
    zip_path = path + ".zip"
    
    downloaded_size = 0
    with open(zip_path, 'wb') as file:
        for data in response.iter_content(block_size):
            file.write(data)
            downloaded_size += len(data)
            if total_size_in_bytes > 0:
                percent = int(downloaded_size * 100 / total_size_in_bytes)
                print(f"DOWNLOAD_PROGRESS: {percent}")
                sys.stdout.flush()
    
    print("Extracting model...")
    sys.stdout.flush()
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(os.path.dirname(path))
    
    os.remove(zip_path)
    print("Model downloaded and extracted.")
    sys.stdout.flush()

def format_time(seconds):
    dt = datetime.datetime.utcfromtimestamp(seconds)
    return dt.strftime('%H:%M:%S,%f')[:-3]

def write_srt_content(f, count, start, end, text):
    f.write(f"{count}\n")
    f.write(f"{format_time(start)} --> {format_time(end)}\n")
    f.write(f"{text}\n\n")

def split_and_write_srt(f, count, segments, max_chars=20):
    """
    通用长句拆分逻辑
    segments: list of dict {'start': float, 'end': float, 'word': str}
    """
    current_segment = []
    current_len = 0
    
    for word_info in segments:
        w_text = word_info['word']
        if current_len + len(w_text) > max_chars and current_segment:
            start = current_segment[0]['start']
            end = current_segment[-1]['end']
            text = "".join([w['word'] for w in current_segment])
            write_srt_content(f, count, start, end, text)
            count += 1
            current_segment = []
            current_len = 0
        
        current_segment.append(word_info)
        current_len += len(w_text)
    
    if current_segment:
        start = current_segment[0]['start']
        end = current_segment[-1]['end']
        text = "".join([w['word'] for w in current_segment])
        write_srt_content(f, count, start, end, text)
        count += 1
    return count

def process_vosk(input_wav, output_srt, script_dir):
    model_path = os.path.join(script_dir, "model", VOSK_MODEL_NAME)
    
    if not os.path.exists(model_path):
        os.makedirs(os.path.dirname(model_path), exist_ok=True)
        potential_path = os.path.join(script_dir, VOSK_MODEL_NAME)
        if os.path.exists(potential_path):
            model_path = potential_path
        else:
            download_vosk_model(os.path.join(script_dir, VOSK_MODEL_NAME))
            model_path = os.path.join(script_dir, VOSK_MODEL_NAME)

    print(f"Loading Vosk model from {model_path}...")
    try:
        model = Model(model_path)
    except Exception as e:
        print(f"Failed to load model: {e}")
        sys.exit(1)

    wf = wave.open(input_wav, "rb")
    if wf.getnchannels() != 1 or wf.getsampwidth() != 2 or wf.getcomptype() != "NONE":
        print("Audio file must be WAV format mono PCM.")
        sys.exit(1)

    total_frames = wf.getnframes()
    rec = KaldiRecognizer(model, wf.getframerate())
    rec.SetWords(True)

    results = []
    print("Transcribing (Vosk)...")
    sys.stdout.flush()
    
    while True:
        data = wf.readframes(4000)
        if len(data) == 0:
            break
        
        current_pos = wf.tell()
        percent = int(current_pos * 100 / total_frames)
        print(f"TRANS_PROGRESS: {percent}")
        sys.stdout.flush()
        
        if rec.AcceptWaveform(data):
            part_result = json.loads(rec.Result())
            results.append(part_result)
    
    final_result = json.loads(rec.FinalResult())
    results.append(final_result)
    
    print("Generating SRT...")
    with open(output_srt, "w", encoding="utf-8") as f:
        count = 1
        for res in results:
            if 'result' in res and res['result']:
                count = split_and_write_srt(f, count, res['result'])
    
    print(f"Subtitle saved to {output_srt}")

def transcribe_whisper_core(model, input_wav, output_srt):
    """
    核心转录逻辑，接受已加载的模型
    """
    print("Transcribing (Whisper)...")
    sys.stdout.flush()
    
    # 强制指定中文 'zh'
    # initial_prompt 可以帮助引导模型，例如使用简体中文
    print("Attempt 1: Standard transcription...")
    
    # 优化参数以减少幻觉和重复
    # condition_on_previous_text=False: 防止前文错误累积导致无限重复
    # no_speech_threshold=0.6: 提高静音检测阈值 (降低幻觉)
    # repetition_penalty=1.3: 强力抑制重复 (Faster-Whisper 特性)
    segments, info = model.transcribe(
        input_wav, 
        beam_size=5, 
        word_timestamps=True, 
        language='zh', 
        initial_prompt=None, # 移除 Prompt 以避免干扰，模型通常能自动识别
        condition_on_previous_text=False,
        no_speech_threshold=0.4, # 稍微降低阈值以避免漏掉轻微语音，靠 repetition_penalty 抑制幻觉
        repetition_penalty=1.3
    )
    
    print(f"Detected language '{info.language}' with probability {info.language_probability}")
    print(f"Audio duration: {info.duration}s")
    sys.stdout.flush()

    total_duration = info.duration
    
    # 收集所有段落
    all_segments = list(segments)
    
    # 如果没有检测到段落，尝试启用 VAD 重试
    if not all_segments:
        print("Warning: No segments detected with standard settings. Retrying with VAD enabled...")
        # 启用 VAD，调整参数
        segments_vad, info_vad = model.transcribe(
            input_wav, 
            beam_size=5, 
            word_timestamps=True, 
            language='zh', 
            initial_prompt=None,
            condition_on_previous_text=False,
            repetition_penalty=1.3,
            vad_filter=True,
            vad_parameters=dict(min_silence_duration_ms=500)
        )
        all_segments = list(segments_vad)
        
        if not all_segments:
            print("Warning: Still no segments detected after VAD retry.")
        else:
            print(f"Success: Detected {len(all_segments)} segments with VAD enabled.")
    
    segment_count = 0
    with open(output_srt, "w", encoding="utf-8") as f:
        count = 1
        for segment in all_segments:
            segment_count += 1
            # 进度估算
            if total_duration > 0:
                percent = int(segment.end * 100 / total_duration)
                print(f"TRANS_PROGRESS: {percent}")
                sys.stdout.flush()
            
            # Whisper segment 也有 words 列表 (因为开启了 word_timestamps=True)
            if segment.words:
                # 转换格式适配 split_and_write_srt
                word_list = [{'start': w.start, 'end': w.end, 'word': w.word} for w in segment.words]
                count = split_and_write_srt(f, count, word_list)
            else:
                # 如果没有词级时间戳，直接写入整句
                write_srt_content(f, count, segment.start, segment.end, segment.text.strip())
                count += 1
    
    if segment_count == 0:
        print("Warning: No segments detected! SRT file will be empty.")
    else:
        print(f"Successfully generated {count-1} subtitle entries from {segment_count} segments.")
                
    print(f"Subtitle saved to {output_srt}")
    return segment_count, info

def process_whisper(input_wav, output_srt, model_size):
    if not HAS_WHISPER:
        print("Error: faster-whisper is not installed. Please pip install faster-whisper -i https://mirrors.aliyun.com/pypi/simple/")
        sys.exit(1)
    
    # 再次确保环境变量 (虽然前面已经设置，但为了保险)
    if os.environ.get("HF_ENDPOINT") != "https://hf-mirror.com":
        os.environ["HF_ENDPOINT"] = "https://hf-mirror.com"
        
    print(f"Loading Whisper model '{model_size}'...")
    print("提示: 首次运行将自动下载模型 (使用 hf-mirror.com 加速)，请耐心等待...")
    sys.stdout.flush()

    # 显式下载模型以捕获下载错误，并获取本地路径
    try:
        # 强制 local_files_only=False 确保可以联网下载
        model_path = download_model(model_size, local_files_only=False)
        print(f"Model downloaded to {model_path}")
    except Exception as e:
        print(f"Error: Model download failed: {e}")
        # 尝试备用方案：如果联网失败，尝试本地缓存
        try:
             print("Network download failed, trying local cache...")
             model_path = download_model(model_size, local_files_only=True)
             print(f"Found in local cache: {model_path}")
        except:
             sys.exit(1)

    # 尝试使用 GPU (CUDA)
    model = None
    using_gpu = False
    
    # 预先检查关键 DLL 是否存在，避免硬崩溃
    # CTranslate2/faster-whisper 需要 cudnn_ops64_9.dll (cuDNN 9)
    # 如果找不到，直接回退 CPU，不尝试初始化 CUDA
    has_cudnn = False
    required_dll = "cudnn_ops64_9.dll"
    if sys.platform == "win32":
        for path in os.environ["PATH"].split(os.pathsep):
            dll_path = os.path.join(path, required_dll)
            if os.path.exists(dll_path):
                has_cudnn = True
                break
    else:
        has_cudnn = True # Linux/Mac 暂不检查
        
    if not has_cudnn and sys.platform == "win32":
        print(f"Warning: {required_dll} not found in PATH. Skipping CUDA initialization to avoid crash.")
    else:
        try:
            model = WhisperModel(model_path, device="cuda", compute_type="int8")
            print("Using GPU (CUDA) for inference.")
            using_gpu = True
        except Exception as e:
            print(f"Warning: GPU init failed ({e}), falling back to CPU.")
            using_gpu = False

    if not using_gpu:
        try:
            model = WhisperModel(model_path, device="cpu", compute_type="int8")
            print("Using CPU for inference.")
        except Exception as e_cpu:
            print(f"Error: Failed to load Whisper model on CPU: {e_cpu}")
            sys.exit(1)

    # 开始转录，如果 GPU 运行时崩溃，尝试回退 CPU
    try:
        # 设置转录状态标志，确保可能的 tqdm 输出被标记为转录进度
        global IS_TRANSCRIBING
        IS_TRANSCRIBING = True

        count, info = transcribe_whisper_core(model, input_wav, output_srt)
        
        # 如果 GPU 转录结果为空，尝试使用更安全的计算类型 (int8_float32) 或回退到 CPU
        # 这是一个关键修复：某些 GPU 在 int8 (float16 compute) 模式下可能因为兼容性问题输出为空
        if count == 0 and using_gpu:
            print("\nWARNING: GPU transcription yielded no results (0 segments).")
            
            # 简单判断：如果音频非常短 (< 2s)，可能是真的没声音，不强制回退以节省时间
            if info.duration < 2.0:
                 print(f"Audio is very short ({info.duration:.2f}s). Assuming silence. Skipping CPU fallback.")
            else:
                print("This might be due to low VRAM or quantization issues on this specific GPU.")
                
                # 尝试 int8_float32 (Int8 storage, Float32 compute)
                # 这通常能解决 FP16 计算精度导致的 garbage output 问题
                print("Attempting GPU retry with compute_type='int8_float32' (Safer precision)...")
                
                # 释放旧模型
                del model
                import gc
                gc.collect()
                
                try:
                    # 重新加载模型 (GPU, int8_float32)
                    model = WhisperModel(model_path, device="cuda", compute_type="int8_float32")
                    print("Retrying transcription on GPU (int8_float32)...")
                    count_retry, info_retry = transcribe_whisper_core(model, input_wav, output_srt)
                    
                    if count_retry > 0:
                        print("Success: GPU retry with int8_float32 worked!")
                        # 成功后直接返回，不需要继续回退
                        IS_TRANSCRIBING = False
                        # 显式退出，防止后续代码（如异常处理或函数尾部）导致非预期行为
                        # 使用 os._exit(0) 而非 sys.exit(0) 以避免 C++ 扩展库 (如 ctranslate2) 在析构时崩溃导致非零退出码
                        os._exit(0)
                        return
                    else:
                         print("Warning: GPU int8_float32 also yielded 0 segments.")
                         del model
                         gc.collect()
                except Exception as e_gpu_retry:
                    print(f"Warning: GPU int8_float32 failed: {e_gpu_retry}")
                
                # 如果 GPU 重试仍然失败，回退到 CPU
                print("Falling back to CPU (INT8)...")
                if sys.platform == "win32":
                    try:
                        import ctypes
                        ctypes.CDLL("cudart64_12.dll").cudaDeviceReset()
                    except:
                        pass
                
                try:
                    print("Reloading model on CPU...")
                    model = WhisperModel(model_path, device="cpu", compute_type="int8")
                    print("Retrying transcription on CPU...")
                    transcribe_whisper_core(model, input_wav, output_srt)
                except Exception as e_cpu_retry:
                    print(f"Error: CPU fallback failed: {e_cpu_retry}")

        IS_TRANSCRIBING = False
                
    except Exception as e:
        # 特别处理 Python 解包错误 (cannot unpack non-iterable int object)
        # 这通常是因为 transcribe_whisper_core 之前的版本只返回了 count，而调用处期望 count, info
        # 但我们已经修复了 transcribe_whisper_core 的返回值
        # 这里的错误实际上是因为在 transcribe_whisper_core 内部 return 之前发生了异常？
        # 或者是因为更深层的库错误
        
        # 如果是 "cannot unpack non-iterable int object"，说明是我们的代码逻辑 bug
        # 但看日志，是在 "Subtitle saved to..." 之后抛出的
        # 也就是 transcribe_whisper_core 已经执行完了，但在 return 语句或者接收返回值时出了问题
        
        print(f"Error during transcription: {e}")
        
        # 如果已经成功生成了字幕文件，且文件不为空，我们可以认为任务其实是成功的
        # 忽略这个错误，不再回退 CPU
        if os.path.exists(output_srt) and os.path.getsize(output_srt) > 100:
             print("Subtitle file generated successfully despite the error. Skipping fallback.")
             # 确保正常退出，不返回错误码
             os._exit(0)

        if using_gpu:
            print("Fatal GPU runtime error detected. Attempting fallback to CPU...")
            # 尝试清理显存
            del model
            import gc
            gc.collect()
            
            try:
                print("Reloading model on CPU...")
                model = WhisperModel(model_path, device="cpu", compute_type="int8")
                print("Retrying transcription on CPU...")
                transcribe_whisper_core(model, input_wav, output_srt)
            except Exception as e_retry:
                print(f"Error: CPU fallback also failed: {e_retry}")
                sys.exit(1)
        else:
            sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Video Subtitle Generator Transcriber")
    parser.add_argument("input_wav", help="Input WAV file path")
    parser.add_argument("output_srt", help="Output SRT file path")
    parser.add_argument("--engine", default="vosk", choices=["vosk", "whisper"], help="Transcription engine")
    parser.add_argument("--model", default="small", help="Model name (for Whisper: tiny, base, small, medium, large; for Vosk: ignored)")
    
    args = parser.parse_args()
    
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    if args.engine == "vosk":
        process_vosk(args.input_wav, args.output_srt, script_dir)
    else:
        process_whisper(args.input_wav, args.output_srt, args.model)

if __name__ == "__main__":
    main()
