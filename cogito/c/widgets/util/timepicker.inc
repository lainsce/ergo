static void cogito_timepicker_ensure_time(CogitoNode* n) {
  if (!n || n->kind != COGITO_TIMEPICKER) return;
  // Default: if all zero, set to current time
  if (n->timepicker.hour == 0 && n->timepicker.minute == 0 && !n->timepicker.ampm) {
    time_t now = time(NULL);
    struct tm* lt = localtime(&now);
    if (lt) {
      n->timepicker.hour = lt->tm_hour;
      n->timepicker.minute = lt->tm_min;
      n->timepicker.ampm = (n->timepicker.hour >= 12);
    }
  }
}

