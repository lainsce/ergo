# GPU Rendering Pipeline Implementation

## Phase 1: Shader System Implementation

- [ ] Add shader format selection based on platform
- [ ] Create vertex shader for colored primitives
- [ ] Create fragment shader for colored primitives
- [ ] Create vertex shader for textured rendering
- [ ] Create fragment shader for textured rendering
- [ ] Add pipeline creation functions
- [ ] Initialize pipelines in sdl3_init()

## Phase 2: Vertex Buffer Flushing

- [ ] Implement flush_vertices() for colored primitives
- [ ] Implement flush_textured_vertices() for text
- [ ] Add pipeline switching logic
- [ ] Call flush in sdl3_end_frame()

## Phase 3: Text Rendering Fix

- [ ] Fix sdl3_draw_text() to use texture sampling
- [ ] Add textured quad drawing
- [ ] Test text rendering

## Phase 4: Scissor Implementation

- [ ] Add scissor state tracking
- [ ] Implement sdl3_begin_scissor()
- [ ] Implement sdl3_end_scissor()
- [ ] Test with scrollable content

## Phase 5: Testing

- [ ] Test with gui_gallery example
- [ ] Verify widgets render correctly
- [ ] Test text rendering
- [ ] Test scissor operations