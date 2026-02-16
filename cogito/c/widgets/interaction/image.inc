static CogitoNode* cogito_image_new_obj(ErgoStr* source) {
  CogitoNode* img = cogito_node_new(COGITO_IMAGE);
  cogito_node_set_icon(img, source);
  img->image.texture = NULL;
  img->image.w = 0;
  img->image.h = 0;
  img->image.loaded = false;
  img->image.avg_lum = 0.5f;
  return img;
}
static ErgoVal cogito_image_new(ErgoVal source) {
  ErgoStr* is = stdr_to_string(source);
  CogitoNode* img = cogito_image_new_obj(is);
  return EV_OBJ(img);
}
static void cogito_image_set_icon(ErgoVal imgv, ErgoVal icon) {
  if (imgv.tag != EVT_OBJ) ergo_trap("cogito.image_set_icon expects image");
  CogitoNode* img = (CogitoNode*)imgv.as.p;
  ErgoStr* is = stdr_to_string(icon);
  
  // Free existing texture if any
  if (img->image.texture && cogito_backend && cogito_backend->texture_destroy) {
    cogito_backend->texture_destroy(img->image.texture);
    img->image.texture = NULL;
  }
  
  cogito_node_set_icon(img, is);
  img->image.loaded = false;
  img->image.w = 0;
  img->image.h = 0;
  img->image.avg_lum = 0.5f;
  cogito_window_relayout(cogito_node_window(img));
}
static void cogito_image_set_source(ErgoVal imgv, ErgoVal source) {
  // Alias for set_icon - both set the image source path
  cogito_image_set_icon(imgv, source);
}
static void cogito_image_set_size(ErgoVal imgv, ErgoVal w, ErgoVal h) {
  if (imgv.tag != EVT_OBJ) ergo_trap("cogito.image_set_size expects image");
  CogitoNode* img = (CogitoNode*)imgv.as.p;
  int width = (int)ergo_as_int(w);
  int height = (int)ergo_as_int(h);
  img->min_w = width;
  img->min_h = height;
  img->min_w_set = true;
  img->min_h_set = true;
  img->max_w = width;
  img->max_h = height;
  img->max_w_set = true;
  img->max_h_set = true;
  cogito_window_relayout(cogito_node_window(img));
}
static void cogito_image_set_radius(ErgoVal imgv, ErgoVal radius) {
  if (imgv.tag != EVT_OBJ) ergo_trap("cogito.image_set_radius expects image");
  CogitoNode* img = (CogitoNode*)imgv.as.p;
  int r = (int)ergo_as_int(radius);
  img->border_radius = r;
  img->radius_set = true;
  cogito_window_relayout(cogito_node_window(img));
}
