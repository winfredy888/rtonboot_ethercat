#!/bin/bash

# 1. Get the absolute path of the script's directory as the main working directory (cross_common)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CROSS_COMMON_DIR="$SCRIPT_DIR"

# 2. Derive the nuttx path and get its absolute path
NUTTX_REL_PATH="../nuttx"
NUTTX_DIR="$(cd "$CROSS_COMMON_DIR/$NUTTX_REL_PATH" && pwd)"

# 3. Define the link description file path (assuming it's in the same directory as the script)
LINKS_FILE="$CROSS_COMMON_DIR/nuttx_links.txt"

# Check if the file exists
if [ ! -f "$LINKS_FILE" ]; then
    echo "Error: Link description file not found: $LINKS_FILE"
    exit 1
fi

echo "Main working directory (cross_common): $CROSS_COMMON_DIR"
echo "NuttX root directory: $NUTTX_DIR"
echo "Processing symbolic links..."

# 4. Read the file line by line
while IFS= read -r line || [ -n "$line" ]; do
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

    # Construct full paths
    # FIX: Both source and target paths are now based on the NuttX root directory
    src_full_path="$NUTTX_DIR/$src_rel_path"
    tgt_full_path="$NUTTX_DIR/$tgt_rel_path"

    # 5. Create or fix symbolic links
    # If the source path already exists (whether it's a regular file, directory, or old symlink), delete it
    if [ -e "$src_full_path" ] || [ -L "$src_full_path" ]; then
        echo "Removing old link/file: $src_full_path"
        rm -rf "$src_full_path"
    fi

    # Ensure the parent directory of the source path exists
    src_parent_dir=$(dirname "$src_full_path")
    if [ ! -d "$src_parent_dir" ]; then
        mkdir -p "$src_parent_dir"
    fi

    # Create the new symbolic link (using the absolute path as the target)
    ln -s "$tgt_full_path" "$src_full_path"
    echo "Link created successfully: $src_full_path -> $tgt_full_path"

done < "$LINKS_FILE"

echo "All symbolic links processed successfully."
