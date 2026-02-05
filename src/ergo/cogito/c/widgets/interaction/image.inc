static CogitoNode* cogito_image_new_obj(ErgoStr* icon) {
  CogitoNode* img = cogito_node_new(COGITO_IMAGE);
  cogito_node_set_icon(img, icon);
  return img;
}
static ErgoVal cogito_image_new(ErgoVal icon) {
  ErgoStr* is = stdr_to_string(icon);
  CogitoNode* img = cogito_image_new_obj(is);
  return EV_OBJ(img);
}
static void cogito_image_set_icon(ErgoVal imgv, ErgoVal icon) {
  if (imgv.tag != EVT_OBJ) ergo_trap("cogito.image_set_icon expects image");
  CogitoNode* img = (CogitoNode*)imgv.as.p;
  ErgoStr* is = stdr_to_string(icon);
  cogito_node_set_icon(img, is);
}
