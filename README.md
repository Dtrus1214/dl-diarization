# DL Diarization (Qt Starter)

A modern Qt Widgets starter application for speaker diarization with:

- Dynamic UI construction (no `.ui` Designer dependency)
- Custom frameless window with rounded corners and custom title bar
- In-layout menu bar under the title bar
- Native standalone diarization engine (no Python runtime required)

## Current Status

This repository is a **starter project**.  
UI and app shell are production-style, while the diarization backend is a native heuristic implementation intended for early integration and UX iteration.

## Project Structure

- `main.cpp` - App bootstrap and application metadata initialization
- `appconfig.h/.cpp` - Product metadata (name, version, build label)
- `mainwindow.h/.cpp` - Dynamic UI, custom window chrome, menu/actions, timeline/log wiring
- `diarizationengine.h/.cpp` - Native async diarization worker and JSON segment output
- `dl-diarization.pro` - qmake project configuration

## Build Requirements

- Qt 5 (Widgets + Concurrent modules)
- C++11 compatible compiler
- qmake + make toolchain

## Build and Run (Windows, qmake)

From project root:

```powershell
qmake "dl-diarization.pro"
mingw32-make -j2
```

Run the generated executable from your build directory.

## How to Use

1. Launch app
2. Import audio file (`.wav` recommended for current native engine)
3. Adjust **Speaker Sensitivity**
4. Click **Run Diarization**
5. Review timeline + logs
6. Export result as JSON

## Engine Notes (Standalone Native Mode)

Current native engine:

- Runs asynchronously using `QtConcurrent`
- Parses WAV directly
- Applies simple energy-based VAD
- Applies heuristic speaker labeling
- Emits JSON segments to UI/export

### Supported Input (current)

- 16-bit PCM WAV (best supported in current implementation)

Other formats may import in UI but are not guaranteed to diarize correctly in native mode yet.

## Output Format

Exported JSON contains:

- `backend` - backend identifier
- `segments` - array of diarization segments:
  - `start` (seconds)
  - `end` (seconds)
  - `speaker` (label string)

## Known Limitations

- Current diarization quality is heuristic (not model-grade)
- No overlap-aware speaker attribution yet
- No confidence score per segment yet
- Native engine currently optimized for PCM WAV path

## Recommended Next Steps

- Introduce pluggable backend interface (`native`, `onnx`, external service)
- Integrate ONNX Runtime C++ diarization pipeline for production accuracy
- Add progress/cancel control and richer timeline visualization
- Add packaging/installer workflow for end-user deployment

## License

Add your project license here (for example, MIT or proprietary internal use).
