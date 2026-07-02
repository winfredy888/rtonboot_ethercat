#!/bin/bash

# 1. Check if a filename parameter is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <link_description_filename>"
    echo "Example: $0 nuttx_rtonboot_links.txt"
    exit 1
fi

# 2. Get the absolute path of the script's directory as the main working directory (cross_common)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CROSS_COMMON_DIR="$SCRIPT_DIR"

# 3. Derive the rtonboot root path and get its absolute path
RTONBOOT_REL_PATH=".."
RTONBOOT_DIR="$(cd "$CROSS_COMMON_DIR/$RTONBOOT_REL_PATH" && pwd)"

# 4. Define the link description file path (file is in the same directory as the script)
LINKS_FILE="$CROSS_COMMON_DIR/$1"

# Check if the file exists
if [ ! -f "$LINKS_FILE" ]; then
    echo "Error: Link description file not found: $LINKS_FILE"
    exit 1
fi

echo "Main working directory (cross_common): $CROSS_COMMON_DIR"
echo "rtonboot root directory: $RTONBOOT_DIR"
echo "Reading links from: $LINKS_FILE"
echo "Processing rtonboot symbolic links..."

# 5. Read the file line by line
while IFS= read -r line || [ -n "$line" ]; do
    # Strip Windows carriage return (\r) from the line
    line="${line//$'\r'/}"

    # Skip empty lines and comments
    [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]] && continue

    # Parse source relative path and target relative path (using ' -> ' as delimiter)
    src_rel_path=$(echo "$line" | awk -F ' -> ' '{print $1}' | xargs)
    tgt_rel_path=$(echo "$line" | awk -F ' -> ' '{print $2}' | xargs)

    # Validate parsing results
    if [ -z "$src_rel_path" ] || [ -z "$tgt_rel_path" ]; then
        echo "Warning: Skipping malformed line: $line"
        continue
    fi

    # Construct full paths based on rtonboot root
    src_full_path="$RTONBOOT_DIR/$src_rel_path"
    # Target is inside cross_common, construct its absolute path
    tgt_full_path="$CROSS_COMMON_DIR/$tgt_rel_path"

    # Extract the directory and the base filename of the source
    src_parent_dir=$(dirname "$src_full_path")
    src_filename=$(basename "$src_full_path")

    # 6. Create or fix symbolic links
    # Ensure the parent directory of the source path exists
    if [ ! -d "$src_parent_dir" ]; then
        mkdir -p "$src_parent_dir"
    fi

    # Enter the source directory
    pushd "$src_parent_dir" > /dev/null || continue

    # If the link/file already exists, delete it
    if [ -e "$src_filename" ] || [ -L "$src_filename" ]; then
        echo "Removing old link/file: $src_filename"
        rm -rf "$src_filename"
    fi

    # Create the symbolic link: ln -s <absolute_target> <relative_link_name>
    ln -s "$tgt_full_path" "./$src_filename"
    echo "Link created successfully: $src_parent_dir/./$src_filename -> $tgt_full_path"

    # Return to the script's original directory
    popd > /dev/null

done < "$LINKS_FILE"

echo "All rtonboot symbolic links processed successfully."
