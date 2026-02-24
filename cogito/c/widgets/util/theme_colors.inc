// Centralized theme color utilities
// Provides consistent access to Ensor scheme colors for widgets that need
// colors not directly available through the SUM stylesheet system.

// Cached tertiary colors - populated when theme is applied via SUM path
// This avoids building Ensor schemes at draw time which can cause timing issues
static CogitoColor cogito_cached_tertiary = {0};
static CogitoColor cogito_cached_on_tertiary = {0};
static CogitoColor cogito_cached_tertiary_container = {0};
static CogitoColor cogito_cached_on_tertiary_container = {0};

// Called from sum_build_dynamic when theme is applied
// Stores tertiary colors for later use by widgets
static void cogito_cache_tertiary_colors(CogitoEnsorScheme* scheme) {
  if (!scheme) return;
  cogito_cached_tertiary = scheme->tertiary;
  cogito_cached_on_tertiary = scheme->on_tertiary;
  cogito_cached_tertiary_container = scheme->tertiary_container;
  cogito_cached_on_tertiary_container = scheme->on_tertiary_container;
}

// Get tertiary colors from cached values (set when theme was applied).
// Uses the SUM path - colors are pre-computed, not built at draw time.
// Any output parameter can be NULL if not needed.
static void cogito_theme_get_tertiary(
  CogitoColor* out_tertiary,
  CogitoColor* out_on_tertiary,
  CogitoColor* out_tertiary_container,
  CogitoColor* out_on_tertiary_container
) {
  if (out_tertiary) *out_tertiary = cogito_cached_tertiary;
  if (out_on_tertiary) *out_on_tertiary = cogito_cached_on_tertiary;
  if (out_tertiary_container) *out_tertiary_container = cogito_cached_tertiary_container;
  if (out_on_tertiary_container) *out_on_tertiary_container = cogito_cached_on_tertiary_container;
}
