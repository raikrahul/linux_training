#!/bin/bash

# Array of module directories in order
DIRS=(
"placeholder"
"module_01_memory_fundamentals"
"module_02_page_fault"
"module_03_allocators"
"module_04_struct_page"
"module_05_advanced_memory"
"module_06_kprobe_tracing"
"module_07_network_tracing"
"module_08_rdma"
"module_09_maple_tree"
"module_10_numa_zones"
)

for i in {1..10}; do
    CURRENT_MOD=${DIRS[$i]}
    if [ $i -lt 10 ]; then
        formatted_i="0$i"
    else
        formatted_i="$i"
    fi
    CURRENT_FILE="$CURRENT_MOD/lesson_$formatted_i.md"
    
    # Previous Link
    if [ $i -eq 1 ]; then
        PREV_LINK="[← Course Index](../index.md)"
    else
        PREV_IDX=$((i-1))
        PREV_MOD=${DIRS[$PREV_IDX]}
        if [ $PREV_IDX -lt 10 ]; then prev_fmt="0$PREV_IDX"; else prev_fmt="$PREV_IDX"; fi
        PREV_LINK="[← Previous Lesson](../$PREV_MOD/lesson_$prev_fmt.md)"
    fi

    # Next Link
    if [ $i -eq 10 ]; then
        NEXT_LINK="[Course Index →](../index.md)"
    else
        NEXT_IDX=$((i+1))
        NEXT_MOD=${DIRS[$NEXT_IDX]}
        if [ $NEXT_IDX -lt 10 ]; then next_fmt="0$NEXT_IDX"; else next_fmt="$NEXT_IDX"; fi
        NEXT_LINK="[Next Lesson →](../$NEXT_MOD/lesson_$next_fmt.md)"
    fi

    # Append
    echo "" >> "$CURRENT_FILE"
    echo "---" >> "$CURRENT_FILE"
    echo "" >> "$CURRENT_FILE"
    echo "$PREV_LINK | [Course Index](../index.md) | $NEXT_LINK" >> "$CURRENT_FILE"
    echo "Added nav to $CURRENT_FILE"
done
