
import sys
import os
import time

# Setup Environment
os.environ["HF_ENDPOINT"] = "https://hf-mirror.com"
os.environ["HF_HUB_DISABLE_SYMLINKS_WARNING"] = "1"

# Add NVIDIA paths
if sys.platform == "win32":
    try:
        import nvidia.cublas
        import nvidia.cudnn
        paths_to_add = []
        cublas_path = os.path.dirname(nvidia.cublas.__file__) if nvidia.cublas.__file__ else list(nvidia.cublas.__path__)[0]
        paths_to_add.append(os.path.join(cublas_path, "bin"))
        cudnn_path = os.path.dirname(nvidia.cudnn.__file__) if nvidia.cudnn.__file__ else list(nvidia.cudnn.__path__)[0]
        paths_to_add.append(os.path.join(cudnn_path, "bin"))
        env_path = os.environ["PATH"]
        for p in paths_to_add:
            if p not in env_path:
                os.environ["PATH"] = p + os.pathsep + os.environ["PATH"]
    except ImportError:
        pass

from faster_whisper import WhisperModel

def test_params(wav_path, label, **kwargs):
    print(f"\n--- Testing: {label} ---")
    print(f"Params: {kwargs}")
    try:
        model = WhisperModel("small", device="cuda", compute_type="int8", download_root=None)
        
        start_t = time.time()
        segments, info = model.transcribe(wav_path, language="zh", **kwargs)
        
        count = 0
        for s in segments:
            count += 1
            if count == 1:
                print(f"  First segment: [{s.start:.2f}->{s.end:.2f}] {s.text}")
            if count >= 5:
                break
        
        print(f"Total segments detected (first 5 checked): {count}")
        if count > 0:
            print("RESULT: SUCCESS")
        else:
            print("RESULT: FAILURE (0 segments)")
            
        del model
    except Exception as e:
        print(f"RESULT: ERROR ({e})")

if __name__ == "__main__":
    # The file that failed for the user
    wav_file = r"D:/BaiduNetdiskDownload/Lua/4-03、c++调用lua函数（基础调用_ev_ev_temp_audio.wav"
    
    if not os.path.exists(wav_file):
        print(f"File not found: {wav_file}")
        # Try the other one that worked previously
        wav_file = r"D:\BaiduNetdiskDownload\Lua\4-06、C++调用Lua函数的错误处理_ev_ev_temp_audio.wav"
        print(f"Trying alternative file: {wav_file}")

    print(f"Target Audio: {wav_file}")

    # 1. Current failing config
    test_params(wav_file, "Current Config", beam_size=5, word_timestamps=True, initial_prompt="简体中文")

    # 2. Disable word_timestamps
    test_params(wav_file, "No Word Timestamps", beam_size=5, word_timestamps=False, initial_prompt="简体中文")

    # 3. Reduce beam_size
    test_params(wav_file, "Beam Size 1", beam_size=1, word_timestamps=True, initial_prompt="简体中文")
    
    # 4. No prompt
    test_params(wav_file, "No Prompt", beam_size=5, word_timestamps=True, initial_prompt=None)
    
    # 6. Test CPU INT8 (Verify file is processable)
    try:
        print("\n--- Testing: CPU INT8 (Control) ---")
        model = WhisperModel("small", device="cpu", compute_type="int8", download_root=None)
        segments, info = model.transcribe(wav_file, language="zh", beam_size=5)
        count = 0
        for s in segments:
            count += 1
            if count == 1:
                print(f"  First segment: [{s.start:.2f}->{s.end:.2f}] {s.text}")
            if count >= 5:
                break
        print(f"CPU Result: {count} segments")
        del model
    except Exception as e:
        print(f"CPU Error: {e}")

    # 7. Test GPU Int8_Float32 (Proposed Fix) - MOVED UP
    print("\n--- Testing: GPU Int8_Float32 (Fix Attempt 2) ---")
    try:
        model = WhisperModel("small", device="cuda", compute_type="int8_float32", download_root=None)
        # Attempt 2: No prompt, repetition penalty
        print("Params: No prompt, repetition_penalty=1.3, condition_on_previous_text=False")
        segments, info = model.transcribe(
            wav_file, 
            language="zh", 
            beam_size=5, 
            condition_on_previous_text=False, 
            initial_prompt=None,
            repetition_penalty=1.3,
            no_speech_threshold=0.4
        )
        count = 0
        for s in segments:
            count += 1
            if count == 1:
                print(f"  First segment: [{s.start:.2f}->{s.end:.2f}] {s.text}")
            if count >= 5:
                break
        print(f"Result: {count} segments")
        del model
    except Exception as e:
        print(f"Error: {e}")

    # 8. Test GPU Float16 (Original Problematic)
    # Skip to save time
    # print("\n--- Testing: GPU Float16 ---")
    
    # 9. Test CPU (Control)
    print("\n--- Testing: CPU INT8 (Control) ---")
    try:
        model = WhisperModel("small", device="cpu", compute_type="int8", download_root=None)
        segments, info = model.transcribe(
            wav_file, 
            language="zh", 
            beam_size=5,
            condition_on_previous_text=False,
            no_speech_threshold=0.6
        )
        count = 0
        for s in segments:
            count += 1
            if count == 1:
                print(f"  First segment: [{s.start:.2f}->{s.end:.2f}] {s.text}")
            if count >= 5:
                break
        print(f"CPU Result: {count} segments")
        del model
    except Exception as e:
        print(f"CPU Error: {e}")
