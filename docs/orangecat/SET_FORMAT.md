# Orange Cat: the set manifest (0.13.3, checks 0.13.4)

A set is the files of one build on the card: the asset pack, the engine module, and for the record the firmware package. A manifest named `*.set.json` in the same folder says which build they are for and which files belong to it. The unit's card browser lists it with the value "Set". OCT+ on it is "Install set".

## What Install set does

1. Reads the manifest. Refuses it ("Not a set") if it is not the format below.
2. Compares the version, then the build's identifiers. "Update firmware first" (set is newer), "Set is for older build", "Set: another build" (same version, other interface id, table address, pack hash or pack length).
3. Looks for every named file and checks it with nothing written ("Missing NAME" if one is gone). (0.13.4) "Checks" means the whole file is read from the card and run through the checks the installer applies at the end: for the pack its header, the payload CRC-32 and the update loader's sector tail rule ("Pack file damaged", "Update would erase it"); for each module the header page against the start and the firmware, the code CRC-32, the page descriptor and the tail rule ("Module header bad", "Module file damaged", "Module pages bad", "Update would erase it"; "Unreadable" for a card error). One bad file refuses the whole set before the first stage, so nothing is erased. Two modules must be able to stand together in the module area ("Modules don't fit").
4. Installs the pack, unless this build's pack is already there.
5. Installs the modules in the manifest's order. When a module must erase the one in the way, the unit asks once per module (OCT+ again on the same row).
6. Says "SET INSTALLED".

The transport must be stopped ("Stop first"). A cut between stages leaves the stages already done in place. The next press skips an identical pack and installs the rest. A module is always installed again.

## The format

The text is what `tools/oc_manifest.py --set` writes (json.dumps, indent 2).

    {
      "version": "0.13.3",
      "pack_hash": 123456789,
      "pack_len": 24576,
      "iface": 2745662291,
      "api_addr": 33558784,
      "files": {
        "pak": "orangecat-0.13.3-abc1234.pak",
        "fwsc": "orangecat-0.13.3-abc1234.fwsc",
        "ocm": ["orangecat-shapes-0.13.3-abc1234.ocm"]
      }
    }

- `version`: "x.y.z", each part 0 to 255. Compared with the firmware's own (VERSION file).
- `pack_hash`, `pack_len`, `iface`, `api_addr`: decimal numbers that fit 32 bits. They must equal the numbers the running firmware was compiled with.
- `files.pak`: required. `files.fwsc`: optional, informational. `files.ocm`: optional, at most 2 names. (0.14.1) A module file may be of either format
  (web/EDITOR_PROTOCOL.md: format 1, an image for each start; format 2, one relocatable image and its list): the card relocates a format 2 image for
  the start it checks (the dry pass: every start the engine may take) and for the start it writes.
- A file name is 1 to 47 plain characters. No `/ \ : * ? < > | "`, no control characters, not starting with a dot. The files are read from the manifest's own folder.
- The reader (`firmware/src/oc_set.h`) is not a general JSON parser. It finds each key and reads the value after it. Every read stays inside the file. The file must be 16 bytes or more and under the text buffer (2,048 bytes).

## Who writes it

`tools/oc_manifest.py --set OUT --pak NAME [--fwsc NAME] [--ocm NAME ...]` from the generated headers. CI writes `<base>.pack.set.json`, `<base>.shapes.set.json` and `<base>.prophet.set.json` into the packages artifact. `web/make_site.py` writes the same three beside the site's files (`firmware/orangecat-V.<choice>.set.json`). One manifest per module choice, because the module area holds SHAPES or the Prophet, not both.

## The firmware package (.fwsc)

The card cannot update the firmware. The update loader ("M-UPGRADE", `ota.c`) takes the package from a host over USB SysEx. There is no card path to it. So a set installs the pack and the modules only. If the firmware is older than the set, the unit says "Update firmware first" and the player updates it from the installer page, then presses Install set.

## Tests

`tests/usb_host_test.c`, section 9: good set, repeat press, pack only, other-build refusals (interface, address, hash, length, newer, older, junk, path, three modules), missing pack, missing module, SHAPES and Prophet together, the same module twice, transport running, no place, a cut between stages, the in-place question, and the parser against every cut and 200,000 mutations of a good manifest. `tests/orangecat_site_test.py`: manifest names and the site's set files.
