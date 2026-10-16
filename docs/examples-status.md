# devkitPro 3ds-examples on Panda3DS + WASM JIT (headless, Null renderer, 90 frames)

27 of 58 examples ran the requested frames without a fatal error.

| example | result | top px | bottom px | ms/frame | first problem | missing JIT ops |
|---|---|---:|---:|---:|---|---|
| app_launch | ran | 1337 | 0 | 2.6 |  |  |
| audio__filters | ran | 2151 | 0 | 21.5 |  |  |
| audio__mic | ran | 0 | 2002 | 20.8 |  |  |
| audio__modplug-decoding | ran | 363 | 0 | 22.0 |  |  |
| audio__ogg-vorbis-decoding | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode UnsignedSaturation |
| audio__opus-decoding | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode SignedSaturation |
| audio__streaming | ran | 873 | 0 | 20.2 |  |  |
| camera__image | ran | 0 | 7291 | 12.6 |  |  |
| camera__video | ran | 0 | 15796 | 2.7 |  |  |
| get_system_language | ran | 0 | 484 | 20.4 |  |  |
| graphics__gpu__2d_shapes | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__both_screens | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__composite_scene | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__cubemap | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__fragment_light | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__geoshader | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__gpusprites | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__immediate | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__lenny | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__loop_subdivision | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__mipmap_fog | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__multiple_buf | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__normal_mapping | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__particles | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__proctex | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__simple_tri | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__stereoscopic_2d | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__textured_cube | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__toon_shading | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__gpu__wide_mode_3d | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__printing__both-screen-text | ran | 1048 | 1317 | 18.3 |  |  |
| graphics__printing__colored-text | ran | 3095 | 0 | 19.0 |  |  |
| graphics__printing__custom-font | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__printing__hello-world | ran | 520 | 0 | 18.1 |  |  |
| graphics__printing__multiple-windows-text | ran | 1959 | 0 | 16.8 |  |  |
| graphics__printing__system-font | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| graphics__printing__wide-console | ran | 3699 | 0 | 18.3 |  |  |
| input__read-controls | ran | 880 | 0 | 18.9 |  |  |
| input__software-keyboard | ran | 2310 | 0 | 17.4 |  |  |
| input__touch-screen | ran | 566 | 0 | 18.5 |  |  |
| libapplet_launch | ran | 3944 | 0 | 20.1 |  |  |
| mii_selector | ran | 2326 | 0 | 19.5 |  |  |
| network__3dslink-demo | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| network__boss | ran | 3233 | 0 | 22.2 |  |  |
| network__http | ran | 0 | 5763 | 2.0 |  |  |
| network__http_post | ran | 0 | 5887 | 1.7 |  |  |
| network__sockets | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| network__sslc | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| network__uds | ran | 1277 | 0 | 19.7 |  |  |
| nfc | exit1 |  |  |  | [FATAL] srv: GetServiceHandle with unknown service nfc:m |  |
| physics__box2d | exit1 |  |  |  | [FATAL] Exit reason: -2147483648 | unsupported IR opcode PackedAddU8 |
| qtm__lenny_qtm_movement_naive | exit1 |  |  |  | [FATAL] Unknown "srv:" command: 000E00C0 |  |
| romfs | ran | 3663 | 0 | 2.3 |  |  |
| sdmc | exit1 |  |  |  | [FATAL] Unimplemented svc: 3 @ 001045A8 |  |
| templates__application | ran | 215 | 0 | 1.8 |  |  |
| threads__event | ran | 794 | 0 | 19.5 |  |  |
| threads__thread-basic | ran | 5296 | 0 | 19.4 |  |  |
| time__rtc | ran | 849 | 0 | 20.1 |  |  |
