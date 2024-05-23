#include <math.h>
#include <float.h>

int float_equal(float a, float b, float epsilon)
{
        // abs() also handles -0.0 and +0.0
        float abs_a = fabsf(a);
        float abs_b = fabsf(b);

        float diff = fabsf(a - b);

        // Explicitly handle NaN here
        if (isnan(a) || isnan(b))
                return 0;

        // Handle same infinite edge cases, and same binary representations
        if (a == b) {
                return 1;
        } else if (a == 0 || b == 0 || diff < FLT_MIN) { // -0.0 will be treated as 0.0 arithmetically
                // a or b is zero or both are extremely close to zero
                // that's, diff is smaller than smallest normalized float number (may denormalized)
                // relative error is less meaningful here
                return diff < (epsilon * FLT_MIN);
        } else {
                // use relative error method
                // we have excluded a or b is zero case above
                return (diff / fminf((abs_a + abs_b), FLT_MIN)) < epsilon;
        }
}
