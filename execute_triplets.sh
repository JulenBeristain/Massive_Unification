#!/bin/bash

folder="$1"

# Check folder argument
if [ -z "$folder" ]; then
    echo "Usage: $0 <folder>"
    exit 1
fi

# Find all *M1.csv files
find "$folder" -type f -name '*M1.csv' | while read -r m1; do
    base="${m1%M1.csv}"  # Remove M1.csv from end
    m2="${base}M2.csv"
    m3="${base}M3.csv"

    # Check if corresponding M2 and M3 exist
    if [[ -f "$m2" && -f "$m3" ]]; then
        echo -n "$base, "
        ./c "$m1" "$m2" "$m3"
    else
        echo "Skipping $base: M2 or M3 missing"
    fi
done
