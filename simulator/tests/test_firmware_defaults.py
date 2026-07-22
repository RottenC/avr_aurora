import re
from pathlib import Path
from avr_aurora_sim import firmware_defaults as defaults

def test_defaults_match_config_header():
    text = Path('src/config.h').read_text()
    for py_name, cpp_name in defaults.CPP_NAMES.items():
        match = re.search(rf'constexpr\s+\w+_t\s+{cpp_name}\s*=\s*(\d+);', text)
        assert match, cpp_name
        assert int(match.group(1)) == getattr(defaults, py_name)
