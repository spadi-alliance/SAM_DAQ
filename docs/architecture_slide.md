**Software Architecture — SAM_DAQ**

- **Diagram:** See [architecture.svg](docs/architecture.svg)
- **One-line purpose:** Capture DAQ decoding and writing pipeline (UI / Decoder / Delegates / Writers)

- **Responsibilities:**
  - **UI:** `MainWindow`, `CLIInterface` — user control and configuration
  - **Core:** `SAMDecoder`, `DelegateManager`, `DelegateWorker` — decode and dispatch samples
  - **Delegates:** `MakeTTreeDecoderDelegate`, `WaveDisplayDecoderDelegate`, `RootTHttpServerDecoderDelegate` — consume decoded data, write/display/serve
  - **Model/IO:** `Board`, `Connection`, ROOT TTree output via `MakeTTreeDecoderDelegate`

- **Data flow:** Input (fakernet / hardware) → `SAMDecoder` → `DelegateManager` → `DelegateWorker` → Delegates → Outputs (ROOT, UI, HTTP)

- **Callout (key class):** `MakeTTreeDecoderDelegate::onAllSamplesDecoded(event, chip, channels, timestamp)` — receives per-event channel samples, fills `TTree`, writes file on `acquisitionWillStop()`.

- **Next steps:** export `docs/architecture.svg` to slide deck (PowerPoint/Google Slides) and refine diagram to show threading or I/O details if needed.
