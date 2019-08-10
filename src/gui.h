void refreshRow(int i, int id, const char* name, const char* summary, int count);
void guiLoop(const char* path, void (*reload)(void));

/* applications define this hook */
extern void gui_recalculate(void* ctx);
