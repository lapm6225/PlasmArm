from dxf_parser import DxfParser
from pprint import pprint

parser = DxfParser("sketch_ligne.dxf")
commands = parser.parse()

# Expand switch points: when config_switch occurs, split into before+after entries
commands = DxfParser.expand_switch_segments(commands)

for c in commands:
    pprint(c)
