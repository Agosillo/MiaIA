# Third-party notices

MiaIA uses third-party components under their own licenses. Those licenses do not change the Mozilla Public License 2.0 terms that apply to original MiaIA source files.

## Native dependencies

| Component | Version used by the current native build | License | Included text |
| --- | --- | --- | --- |
| ONNX | 1.19.0 | Apache License 2.0 | [Apache-2.0.txt](LICENSES/Apache-2.0.txt) |
| Protocol Buffers | 6.33.4 | BSD-style license | [Protobuf.txt](LICENSES/Protobuf.txt) |
| Abseil | 20260107.1 | Apache License 2.0 | [Apache-2.0.txt](LICENSES/Apache-2.0.txt) |
| utf8-range | transitive dependency | MIT License | [utf8-range.txt](LICENSES/utf8-range.txt) |

ONNX is an open standard for machine-learning interoperability. Use of the ONNX name does not imply endorsement of MiaIA.

## Packaged Unreal runtime

Depending on the Unreal Engine version, target, enabled plugins, and packaging configuration, a MiaIA Studio package may contain additional runtime components. The current Windows package includes or may include ONNX Runtime and oneTBB:

| Component | License | Included text |
| --- | --- | --- |
| ONNX Runtime | MIT License and bundled third-party notices | [ONNX-Runtime.txt](LICENSES/ONNX-Runtime.txt) |
| oneTBB | Apache License 2.0 | [oneTBB.txt](LICENSES/oneTBB.txt) |

Unreal Engine is licensed separately by Epic Games. Epic's third-party runtime notices generated during packaging must be retained in the distributed archive. The authoritative additional license material for an installed Engine version is located in its `Engine/Source/ThirdParty/Licenses` and `Engine/Extras/ThirdPartyNotUE` directories.

The release packaging workflow copies this notice, the MiaIA license, and the listed license texts into the packaged application's `Licenses` directory. A release owner must still review the final staged file inventory whenever the Unreal version, plugins, or runtime dependencies change.

## Unreal Engine attribution

MiaIA Studio uses Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United States of America and elsewhere.

Unreal® Engine, Copyright 1998–2026, Epic Games, Inc. All rights reserved.
