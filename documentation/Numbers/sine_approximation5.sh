#!/usr/bin/env bash
set -euo pipefail

# Check if sollya is available
if ! command -v sollya &> /dev/null; then
    echo "Error: sollya not found. Please install it first."
    exit 1
fi

echo "Generating cubic minimax approximation for sin(x*π/2)..."

# Get system temp directory
TMPDIR="${TMPDIR:-/tmp}"
SCRIPT_FILE="$TMPDIR/sollya_$$.sly"

# Create sollya script
cat > "$SCRIPT_FILE" << 'EOF'
prec = 31;
f = sin(x);
p = remez(f, 5, [-pi/2, pi/2]);
coeff0 = coeff(p, 0);
coeff1 = coeff(p, 1);
coeff2 = coeff(p, 2);
coeff3 = coeff(p, 3);
coeff4 = coeff(p, 4);
coeff5 = coeff(p, 5);

// Create simplified odd-only polynomial
p_simplified = coeff1 * x + coeff3 * x^3+ coeff5 * x^5;

write("Full polynomial: ", p, "\n");
write("Simplified polynomial (odd terms only): ", p_simplified, "\n\n");

full_error = dirtyinfnorm(f - p, [-pi/2, pi/2]);
simplified_error = dirtyinfnorm(f - p_simplified, [-pi/2, pi/2]);

write("Error comparison:\n");
write("  Full polynomial error:       ", full_error, "\n");
write("  Simplified polynomial error: ", simplified_error, "\n");
write("  Error increase factor:       ", simplified_error / full_error, "\n\n");
quit;
EOF

# Run sollya and cleanup on any exit
trap 'rm -f "$SCRIPT_FILE"' EXIT

sollya --flush --noprompt "$SCRIPT_FILE"