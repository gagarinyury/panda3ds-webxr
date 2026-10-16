# devkitPro 3ds-examples on Panda3DS + WASM JIT (headless, Null renderer, 90 frames)

54 of 58 examples ran the requested frames without a fatal error.

| example | result | top px | bottom px | ms/frame | first problem | missing JIT ops |
|---|---|---:|---:|---:|---|---|
| app_launch | ran | 1337 | 0 | 1.4 |  |  |
| audio__filters | ran | 2151 | 0 | 1.8 |  |  |
| audio__mic | ran | 0 | 2002 | 1.6 |  |  |
| audio__modplug-decoding | ran | 363 | 0 | 4.1 |  |  |
| audio__ogg-vorbis-decoding | ran | 3649 | 0 | 4.4 |  |  |
| audio__opus-decoding | ran | 4323 | 0 | 5.6 |  |  |
| audio__streaming | ran | 873 | 0 | 1.7 |  |  |
| camera__image | ran | 0 | 7291 | 2.6 |  |  |
| camera__video | ran | 0 | 15796 | 1.7 |  |  |
| get_system_language | ran | 0 | 484 | 1.6 |  |  |
| graphics__gpu__2d_shapes | ran | 0 | 1132 | 3.9 |  |  |
| graphics__gpu__both_screens | ran | 0 | 0 | 2.0 |  |  |
| graphics__gpu__composite_scene | ran | 0 | 0 | 3.0 |  |  |
| graphics__gpu__cubemap | ran | 0 | 0 | 2.6 |  |  |
| graphics__gpu__fragment_light | ran | 0 | 0 | 2.0 |  |  |
| graphics__gpu__geoshader | ran | 0 | 0 | 1.8 |  |  |
| graphics__gpu__gpusprites | ran | 0 | 1906 | 5.4 |  |  |
| graphics__gpu__immediate | ran | 0 | 0 | 1.7 |  |  |
| graphics__gpu__lenny | ran | 0 | 0 | 2.2 |  |  |
| graphics__gpu__loop_subdivision | ran | 0 | 0 | 2.0 |  |  |
| graphics__gpu__mipmap_fog | ran | 0 | 0 | 2.6 |  |  |
| graphics__gpu__multiple_buf | ran | 0 | 0 | 1.8 |  |  |
| graphics__gpu__normal_mapping | ran | 0 | 0 | 2.3 |  |  |
| graphics__gpu__particles | ran | 0 | 1905 | 3.7 |  |  |
| graphics__gpu__proctex | ran | 0 | 518 | 2.9 |  |  |
| graphics__gpu__simple_tri | ran | 0 | 0 | 1.8 |  |  |
| graphics__gpu__stereoscopic_2d | ran | 0 | 671 | 3.3 |  |  |
| graphics__gpu__textured_cube | ran | 0 | 0 | 2.1 |  |  |
| graphics__gpu__toon_shading | ran | 0 | 0 | 2.4 |  |  |
| graphics__gpu__wide_mode_3d | ran | 0 | 0 | 2.2 |  |  |
| graphics__printing__both-screen-text | ran | 1048 | 1317 | 1.5 |  |  |
| graphics__printing__colored-text | ran | 3095 | 0 | 1.6 |  |  |
| graphics__printing__custom-font | exit1 |  |  |  | [FATAL] [NCCH archive] Read unimplemented NAND file. ID: 00014202 |  |
| graphics__printing__hello-world | ran | 520 | 0 | 1.5 |  |  |
| graphics__printing__multiple-windows-text | ran | 1959 | 0 | 1.7 |  |  |
| graphics__printing__system-font | ran | 0 | 0 | 4.4 |  |  |
| graphics__printing__wide-console | ran | 3699 | 0 | 1.6 |  |  |
| input__read-controls | ran | 880 | 0 | 1.5 |  |  |
| input__software-keyboard | ran | 2310 | 0 | 1.4 |  |  |
| input__touch-screen | ran | 566 | 0 | 1.7 |  |  |
| libapplet_launch | ran | 3944 | 0 | 1.4 |  |  |
| mii_selector | ran | 2326 | 0 | 1.5 |  |  |
| network__3dslink-demo | ran | 1117 | 0 | 1.5 |  |  |
| network__boss | ran | 3233 | 0 | 1.7 |  |  |
| network__http | ran | 0 | 5763 | 1.6 |  |  |
| network__http_post | ran | 0 | 5887 | 1.7 |  |  |
| network__sockets | ran | 945 | 0 | 4.4 |  |  |
| network__sslc | ran | 1319 | 0 | 1.5 |  |  |
| network__uds | ran | 1277 | 0 | 1.6 |  |  |
| nfc | exit1 |  |  |  | [FATAL] srv: GetServiceHandle with unknown service nfc:m |  |
| physics__box2d | ran | 0 | 0 | 4.2 |  |  |
| qtm__lenny_qtm_movement_naive | exit1 |  |  |  | [FATAL] Unknown "srv:" command: 000E00C0 |  |
| romfs | ran | 3663 | 0 | 2.1 |  |  |
| sdmc | exit1 |  |  |  | [FATAL] Unimplemented svc: 3 @ 001045A8 |  |
| templates__application | ran | 215 | 0 | 1.3 |  |  |
| threads__event | ran | 794 | 0 | 1.8 |  |  |
| threads__thread-basic | ran | 5297 | 0 | 1.5 |  |  |
| time__rtc | ran | 842 | 0 | 1.7 |  |  |
