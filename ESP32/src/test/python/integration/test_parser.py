
from dxf_parser import DxfParser

filename=input("Enter DXF filename: ")
parser = DxfParser(filename)
commands = parser.parse()
print(commands  )

parser.print_preview(commands)