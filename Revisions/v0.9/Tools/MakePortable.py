"""Build the v0.9 portable Windows ZIP with content hashes and CRC verification."""
from pathlib import Path
import importlib.util
import json
module_path = Path(__file__).with_name("MakeHandoff-v09.py")
spec = importlib.util.spec_from_file_location("chronicles_handoff", module_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
output = module.ROOT / "Builds/Chronicles-LAN-Windows.zip"
print(json.dumps(module.build_portable(output), indent=2))
