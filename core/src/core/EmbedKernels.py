#
# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import glob
import sys

# Parse command line arguments.
source_dir = sys.argv[1]
output_file_name = sys.argv[2]

# Make a list of all the .cl files in the directory.
print("Looking for kernels in " + source_dir + "...")
files = glob.glob(source_dir + "/*.cl")

# Open the output file.
output_file = open(output_file_name, 'w')

# Read the contents of each file into a string.
output_file.write("static const char gKernelSource[] = \\\n")
for file_name in files:
    print("Found " + file_name)
    lines = open(file_name).readlines()
    for line in lines:
        output_file.write("\"" + line.strip("\r\n").replace("\"", "\\\"") + "\\n\"\\\n")
output_file.write(";\n")
output_file.close()

print("Kernels embedded into " + output_file_name)
