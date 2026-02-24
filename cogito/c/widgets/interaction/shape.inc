static float cogito_shape_clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static void cogito_shape_apply_preset_node(CogitoNode* n, int preset) {
  if (!n) return;
  if (preset < 0) preset = 0;
  if (preset >= COGITO_SHAPE_COUNT) preset = COGITO_SHAPE_CIRCLE;
  n->shape.preset = preset;

  float r[8] = {1, 1, 1, 1, 1, 1, 1, 1};
  float sx = 1.0f;
  float sy = 1.0f;
  float rot = 0.0f;
  float skew = 0.0f;
  float yoff = 0.0f;
  bool pixel = false;

  switch (preset) {
    case COGITO_SHAPE_CIRCLE:
      break;
    case COGITO_SHAPE_SQUARE:
      r[0] = 0.96f; r[1] = 1.00f; r[2] = 0.96f; r[3] = 1.00f;
      r[4] = 0.96f; r[5] = 1.00f; r[6] = 0.96f; r[7] = 1.00f;
      break;
    case COGITO_SHAPE_SLANTED:
      r[1] = 1.05f; r[2] = 1.05f; r[5] = 0.92f; r[6] = 0.92f;
      skew = 0.22f;
      break;
    case COGITO_SHAPE_ARCH:
      r[4] = 0.72f; r[5] = 0.74f; r[6] = 0.74f; r[7] = 0.72f;
      sy = 1.05f;
      break;
    case COGITO_SHAPE_SEMICIRCLE:
      r[4] = 0.55f; r[5] = 0.58f; r[6] = 0.58f; r[7] = 0.55f;
      sy = 1.02f;
      break;
    case COGITO_SHAPE_OVAL:
      sx = 0.82f; sy = 1.10f; rot = -0.34f;
      break;
    case COGITO_SHAPE_PILL:
      sx = 0.90f; sy = 1.02f; rot = 0.55f;
      r[0] = 0.94f; r[4] = 0.94f;
      break;
    case COGITO_SHAPE_TRIANGLE:
      r[0] = 1.00f; r[1] = 0.84f; r[2] = 0.70f; r[3] = 0.80f;
      r[4] = 0.98f; r[5] = 0.80f; r[6] = 0.70f; r[7] = 0.84f;
      break;
    case COGITO_SHAPE_ARROW:
      r[0] = 1.00f; r[1] = 0.82f; r[2] = 0.72f; r[3] = 0.54f;
      r[4] = 0.60f; r[5] = 0.54f; r[6] = 0.72f; r[7] = 0.82f;
      break;
    case COGITO_SHAPE_FAN:
      r[0] = 1.00f; r[1] = 1.00f; r[2] = 0.88f; r[3] = 0.62f;
      r[4] = 0.62f; r[5] = 0.62f; r[6] = 0.62f; r[7] = 0.62f;
      rot = 0.42f;
      break;
    case COGITO_SHAPE_DIAMOND:
      r[0] = 1.00f; r[1] = 0.72f; r[2] = 1.00f; r[3] = 0.72f;
      r[4] = 1.00f; r[5] = 0.72f; r[6] = 1.00f; r[7] = 0.72f;
      break;
    case COGITO_SHAPE_CLAMSHELL:
      r[0] = 0.92f; r[1] = 0.78f; r[2] = 0.92f; r[3] = 0.78f;
      r[4] = 0.92f; r[5] = 0.78f; r[6] = 0.92f; r[7] = 0.78f;
      sx = 1.12f; sy = 0.86f;
      break;
    case COGITO_SHAPE_PENTAGON:
      r[0] = 1.00f; r[1] = 0.88f; r[2] = 0.86f; r[3] = 0.84f;
      r[4] = 0.86f; r[5] = 0.84f; r[6] = 0.86f; r[7] = 0.88f;
      break;
    case COGITO_SHAPE_GEM:
      r[0] = 1.00f; r[1] = 0.90f; r[2] = 0.92f; r[3] = 0.96f;
      r[4] = 0.94f; r[5] = 0.96f; r[6] = 0.92f; r[7] = 0.90f;
      break;
    case COGITO_SHAPE_VERY_SUNNY:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 1.00f : 0.72f;
      break;
    case COGITO_SHAPE_SUNNY:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.98f : 0.80f;
      break;
    case COGITO_SHAPE_COOKIE_4:
      r[0] = 0.90f; r[1] = 0.78f; r[2] = 0.90f; r[3] = 0.78f;
      r[4] = 0.90f; r[5] = 0.78f; r[6] = 0.90f; r[7] = 0.78f;
      break;
    case COGITO_SHAPE_COOKIE_6:
      r[0] = 0.90f; r[1] = 0.84f; r[2] = 0.90f; r[3] = 0.84f;
      r[4] = 0.90f; r[5] = 0.84f; r[6] = 0.90f; r[7] = 0.84f;
      break;
    case COGITO_SHAPE_COOKIE_7:
      r[0] = 0.92f; r[1] = 0.86f; r[2] = 0.90f; r[3] = 0.86f;
      r[4] = 0.90f; r[5] = 0.86f; r[6] = 0.90f; r[7] = 0.86f;
      break;
    case COGITO_SHAPE_COOKIE_9:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.92f : 0.88f;
      break;
    case COGITO_SHAPE_COOKIE_12:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.90f : 0.87f;
      break;
    case COGITO_SHAPE_CLOVER_4:
      r[0] = 0.92f; r[1] = 1.00f; r[2] = 0.92f; r[3] = 1.00f;
      r[4] = 0.92f; r[5] = 1.00f; r[6] = 0.92f; r[7] = 1.00f;
      break;
    case COGITO_SHAPE_CLOVER_8:
      for (int i = 0; i < 8; i++) r[i] = 1.00f;
      sx = 0.96f;
      sy = 0.96f;
      break;
    case COGITO_SHAPE_BURST:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 1.00f : 0.62f;
      rot = 0.20f;
      break;
    case COGITO_SHAPE_SOFT_BURST:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.98f : 0.72f;
      rot = 0.20f;
      break;
    case COGITO_SHAPE_BOOM:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 1.00f : 0.52f;
      break;
    case COGITO_SHAPE_SOFT_BOOM:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.98f : 0.70f;
      break;
    case COGITO_SHAPE_FLOWER:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.96f : 0.76f;
      sy = 1.05f;
      break;
    case COGITO_SHAPE_PUFFY:
      for (int i = 0; i < 8; i++) r[i] = (i % 2 == 0) ? 0.88f : 0.94f;
      sx = 1.08f;
      sy = 0.92f;
      break;
    case COGITO_SHAPE_PUFFY_DIAMOND:
      r[0] = 1.00f; r[1] = 0.78f; r[2] = 0.90f; r[3] = 0.78f;
      r[4] = 1.00f; r[5] = 0.78f; r[6] = 0.90f; r[7] = 0.78f;
      break;
    case COGITO_SHAPE_GHOSTISH:
      r[0] = 1.00f; r[1] = 0.92f; r[2] = 0.90f; r[3] = 0.86f;
      r[4] = 0.66f; r[5] = 0.78f; r[6] = 0.66f; r[7] = 0.78f;
      yoff = -0.03f;
      break;
    case COGITO_SHAPE_PIXEL_CIRCLE:
      pixel = true;
      break;
    case COGITO_SHAPE_PIXEL_TRIANGLE:
      r[0] = 1.00f; r[1] = 0.84f; r[2] = 0.70f; r[3] = 0.84f;
      r[4] = 0.70f; r[5] = 0.84f; r[6] = 0.70f; r[7] = 0.84f;
      pixel = true;
      break;
    case COGITO_SHAPE_BUN:
      r[0] = 0.92f; r[1] = 0.80f; r[2] = 0.92f; r[3] = 0.80f;
      r[4] = 0.80f; r[5] = 0.96f; r[6] = 0.80f; r[7] = 0.96f;
      sy = 0.92f;
      break;
    case COGITO_SHAPE_HEART:
      r[0] = 0.34f; r[1] = 0.96f; r[2] = 0.94f; r[3] = 0.96f;
      r[4] = 1.00f; r[5] = 0.70f; r[6] = 0.56f; r[7] = 0.70f;
      yoff = 0.03f;
      break;
    default:
      break;
  }

  const float base = -1.57079632679f;
  for (int i = 0; i < 8; i++) {
    float a = base + ((float)i * 0.78539816339f) + rot;
    float ca = cosf(a);
    float sa = sinf(a);
    float rr = r[i] * 0.5f;
    float px = ca * rr * sx + sa * rr * skew;
    float py = sa * rr * sy;
    float x = 0.5f + px * 0.9f;
    float y = 0.5f + py * 0.9f + yoff;
    x = cogito_shape_clamp01(x);
    y = cogito_shape_clamp01(y);
    if (pixel) {
      x = lroundf(x * 12.0f) / 12.0f;
      y = lroundf(y * 12.0f) / 12.0f;
    }
    n->shape.vertices[i * 2 + 0] = x;
    n->shape.vertices[i * 2 + 1] = y;
  }
}

static CogitoNode* cogito_shape_new_obj(int preset) {
  CogitoNode* n = cogito_node_new(COGITO_SHAPE);
  cogito_shape_apply_preset_node(n, preset);
  return n;
}

static ErgoVal cogito_shape_new(ErgoVal presetv) {
  int preset = (int)ergo_as_int(presetv);
  return EV_OBJ(cogito_shape_new_obj(preset));
}

static void cogito_shape_set_preset(ErgoVal shapev, ErgoVal presetv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_preset expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int preset = (int)ergo_as_int(presetv);
  cogito_shape_apply_preset_node(n, preset);
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_shape_get_preset(ErgoVal shapev) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_preset expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  return EV_INT(n->shape.preset);
}

static void cogito_shape_set_vertex(ErgoVal shapev, ErgoVal idxv, ErgoVal xv, ErgoVal yv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_set_vertex expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0 || idx >= 8) return;
  float x = (float)ergo_as_float(xv);
  float y = (float)ergo_as_float(yv);
  n->shape.vertices[idx * 2 + 0] = cogito_shape_clamp01(x);
  n->shape.vertices[idx * 2 + 1] = cogito_shape_clamp01(y);
  cogito_window_relayout(cogito_node_window(n));
}

static ErgoVal cogito_shape_get_vertex_x(ErgoVal shapev, ErgoVal idxv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_vertex_x expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0 || idx >= 8) return EV_FLOAT(0.0);
  return EV_FLOAT(n->shape.vertices[idx * 2 + 0]);
}

static ErgoVal cogito_shape_get_vertex_y(ErgoVal shapev, ErgoVal idxv) {
  if (shapev.tag != EVT_OBJ) ergo_trap("cogito.shape_get_vertex_y expects shape");
  CogitoNode* n = (CogitoNode*)shapev.as.p;
  int idx = (int)ergo_as_int(idxv);
  if (idx < 0 || idx >= 8) return EV_FLOAT(0.0);
  return EV_FLOAT(n->shape.vertices[idx * 2 + 1]);
}
