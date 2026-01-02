
import os
import sys
import time

# 1. Setup Environment
os.environ["HF_ENDPOINT"] = "https://hf-mirror.com"
if "HF_HUB_OFFLINE" in os.environ: del os.environ["HF_HUB_OFFLINE"]
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

try:
    from faster_whisper import WhisperModel
    import ctranslate2
    print(f"CTranslate2 Version: {ctranslate2.__version__}")
except ImportError:
    print("Error: faster-whisper not installed.")
    sys.exit(1)

def test_transcription(wav_path, device, compute_type, model_size="tiny"):
    print(f"\n--- Testing Device: {device} | Compute: {compute_type} ---")
    try:
        start_t = time.time()
        model = WhisperModel(model_size, device=device, compute_type=compute_type, download_root=None)
        load_t = time.time()
        print(f"Model loaded in {load_t - start_t:.2f}s")
        
        segments, info = model.transcribe(wav_path, beam_size=1, language="zh")
        
        count = 0
        print("Segments:")
        for s in segments:
            count += 1
            print(f"  [{s.start:.2f}->{s.end:.2f}] {s.text}")
            if count >= 3:
                print("  ... (stopping early for test)")
                break
        
        print(f"Total segments detected: {count}")
        if count == 0:
            print("FAILURE: No segments detected.")
        else:
            print("SUCCESS: Transcription working.")
            
        del model
    except Exception as e:
        print(f"ERROR: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        wav_file = sys.argv[1]
    else:
        # Default fallback for testing
        wav_file = r"D:\BaiduNetdiskDownload\Lua\4-06、C++调用Lua函数的错误处理_ev_ev_temp_audio.wav"
    
    if not os.path.exists(wav_file):
        print(f"File not found: {wav_file}")
        sys.exit(1)
        
    print(f"Target Audio: {wav_file}")
    
    # Test 1: GPU INT8 (The failing case)
    test_transcription(wav_file, "cuda", "int8")
    
    # Test 2: GPU Float16 (Alternative)
    test_transcription(wav_file, "cuda", "float16")
    
    # Test 3: GPU Float32 (High precision, high memory)
    test_transcription(wav_file, "cuda", "float32")
    
    # Test 4: CPU INT8 (The working case)
    test_transcription(wav_file, "cpu", "int8")
