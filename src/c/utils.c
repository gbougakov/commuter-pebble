#include "utils.h"

// Helper function to abbreviate station names
void abbreviate_station_name(const char *input, char *output, size_t output_size) {
  if (!input || !output || output_size == 0) return;

  // Check for prefixes that need abbreviation
  if (strstr(input, "Antwerp") || strstr(input, "Antwerpen") || strstr(input, "Anvers")) {
    // Antwerp-Central → Antw-C, Antwerp-Berchem → Antw-Berchem
    snprintf(output, output_size, "Antw%s", strchr(input, '-') ? strchr(input, '-') : "");
  } else if (strstr(input, "Brussels") || strstr(input, "Brussel") || strstr(input, "Bruxelles")) {
    // Special case: Brussels Airport-Zaventem → Bru-Airport
    if (strstr(input, "Airport")) {
      snprintf(output, output_size, "Bru-Airport");
    } else {
      // Brussels-Central → Bru-Central, Brussels-South → Bru-South
      snprintf(output, output_size, "Bru%s", strchr(input, '-') ? strchr(input, '-') : "");
    }
  } else if (strncmp(input, "Charleroi-", 10) == 0) {
    // Charleroi-South → Crl-South
    snprintf(output, output_size, "Crl%s", strchr(input, '-'));
  } else if (strncmp(input, "Mechelen-", 9) == 0) {
    // Mechelen-Nekkerspoel → M-Nekkerspoel (but "Mechelen" stays as is)
    snprintf(output, output_size, "M%s", strchr(input, '-'));
  } else if (strncmp(input, "Liège-", 6) == 0 || strncmp(input, "Liége-", 6) == 0) {
    // Liège-Guillemins → L-Guillemins
    snprintf(output, output_size, "L%s", strchr(input, '-'));
  } else {
    // No abbreviation needed
    strncpy(output, input, output_size - 1);
    output[output_size - 1] = '\0';
  }
}
