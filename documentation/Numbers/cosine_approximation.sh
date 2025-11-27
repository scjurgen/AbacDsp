#!/usr/bin/env bash
set -euo pipefail

# Check if sollya is available
if ! command -v sollya &> /dev/null; then
    echo "Error: sollya not found. Please install it first."
    exit 1
fi

echo "Generating cosine approximation similar to qcos..."

TMPDIR="${TMPDIR:-/tmp}"
SCRIPT_FILE="$TMPDIR/sollya_cos_$$.sly"

cat > "$SCRIPT_FILE" << 'EOF'
prec = 31;

f1 = cos(x);
p1 = remez(f1, 4, [-1, 1]);
// Extract even coefficients (cos is an even function)
coeff0_1 = coeff(p1, 0);
coeff2_1 = coeff(p1, 2);
coeff4_1 = coeff(p1, 4);

write("===  cos(x) on [-1, 1] ===\n");
write("Full polynomial: ", p1, "\n");
write("Even coefficients only: ", coeff0_1, " + ", coeff2_1, "*x^2 + ", coeff4_1, "*x^4\n");

p1_even = coeff0_1 + coeff2_1 * x^2 + coeff4_1 * x^4;
error1_full = dirtyinfnorm(f1 - p1, [-1, 1]);
error1_even = dirtyinfnorm(f1 - p1_even, [-1, 1]);

write("Error (full):     ", error1_full, "\n");
write("Error (even only): ", error1_even, "\n\n");

// Option 2: cos(x) on [-pi/2, pi/2]
f2 = cos(x*pi/2);
p2 = remez(f2, 4, [-pi/2, pi/2]);

coeff0_2 = coeff(p2, 0);
coeff1_2 = coeff(p2, 1);
coeff2_2 = coeff(p2, 2);
coeff3_2 = coeff(p2, 3);
coeff4_2 = coeff(p2, 4);

write("=== Option 2: cos(x) on [-pi/2, pi/2] ===\n");
write("Full polynomial: ", p2, "\n");
write("Even coefficients only: ", coeff0_2, " + ", coeff2_2, "*x^2 + ", coeff4_2, "*x^4\n");
p2_custom = coeff0_2 + coeff2_2 * x^2 + coeff4_2 * abs(x * x^2);
error2_full = dirtyinfnorm(f2 - p2, [0, pi/2]);

quit;
EOF

# Run sollya and cleanup on any exit
trap 'rm -f "$SCRIPT_FILE"' EXIT

sollya --flush --noprompt "$SCRIPT_FILE"