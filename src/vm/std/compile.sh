#!/bin/bash

# Output C++ file
output_file="embedded.h"

# Start writing the C++ file
cat <<EOL > "$output_file"
#include <map>
#include <string>

namespace mmx {
namespace vm {

std::map<std::string, std::string> std_file_map = {
EOL

# Iterate over all *.js files in the current directory
for js_file in *.js; do
    if [[ -f "$js_file" ]]; then
        # Read the file content and escape double quotes and newlines
        content=$(sed ':a;N;$!ba;s/"/\\"/g;s/\n/\\n/g' "$js_file")
        # Append the file content to the C++ map
        echo "    {\"$js_file\", \"$content\"}," >> "$output_file"
    fi
done

# Close the map and namespace
cat <<EOL >> "$output_file"
};

} // namespace vm
} // namespace mmx
EOL
