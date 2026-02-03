from pygcode import Line

with open('test.gcode', 'r') as fh:
    for line_text in fh.readlines():
        line = Line(line_text)
        
        #print(line)  # will print the line (with cosmetic changes)
        print(line.block.gcodes) # is your list of gcodes
        #print(line.block.modal_params)  # are all parameters not assigned to a gcode, assumed to be motion modal parameters
        if line.comment:
            print(line.comment.text)  # your comment text
print("------")