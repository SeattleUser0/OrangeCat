The first 60 bytes (fw_mod_hdr_t) of the SHAPES, WHEEL and Prophet modules as Release 2's last build made them: linux-hw fd159be (round 14, interface identifier 0xCC9AED54),
`ci_step.py build`, build/module/<name>.<start>.img (start 0). Only the header is kept: fw_module_check refuses a module on the header (the identifier), before any other byte is read.
tests/mod_iface_r3_test.c checks that a Release 3 firmware refuses these three and accepts the same modules built for it.
