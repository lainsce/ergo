// Draw queue: only redraw when something requested it (queue_draw) or layout actually changed.
// Used by the run loop to avoid redraws when idle; relayout calls queue_draw only when geometry changed.

static CogitoNode* cogito_draw_pending_window = NULL;
static bool cogito_layout_did_change = false;

static void cogito_queue_draw(CogitoNode* win) {
  if (!win) return;
  cogito_draw_pending_window = win;
}

static bool cogito_draw_pending(CogitoNode* win) {
  return win && (cogito_draw_pending_window == win);
}

static void cogito_draw_clear_pending(CogitoNode* win) {
  if (cogito_draw_pending_window == win) cogito_draw_pending_window = NULL;
}

static void cogito_layout_mark_changed(void) {
  cogito_layout_did_change = true;
}

static void cogito_layout_begin(void) {
  cogito_layout_did_change = false;
}

static bool cogito_layout_changed(void) {
  return cogito_layout_did_change;
}
