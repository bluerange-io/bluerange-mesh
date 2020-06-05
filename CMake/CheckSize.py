import subprocess 
import sys

output = subprocess.check_output([sys.argv[1], sys.argv[2]])
tokens = output.split()
total_size = int(tokens[6]) + int(tokens[7])
max_size = int(sys.argv[3])
fail_on_size_too_big = int(sys.argv[4])
print("Total size is " + str(total_size))

if total_size > max_size:
    if fail_on_size_too_big > 0:
        print("!FATAL ERROR! Firmware size is too big for updating over the mesh. Max Size is " + str(max_size) + " but " + str(total_size) + " was used!FATAL ERROR!")
        sys.exit(1)
    else:
        print("!WARNING! Firmware will be too big for updating over the mesh.")
        print("!WARNING! To solve this, undef some things in your featureset.")