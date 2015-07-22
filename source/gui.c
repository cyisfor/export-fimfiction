#include <gtk/gtk.h>
#include <glib.h>

extern void reloadFile(void*,void (*)(void));
extern const char* getContents(int);

static GtkLabel* makeLabel() {
  GtkWidget* label = gtk_label_new("...");
  gtk_label_set_justify(GTK_LABEL(label),GTK_JUSTIFY_LEFT);
  g_object_widget_set_property(label,"halign",GTK_ALIGN_START);
  g_object_widget_set_property(label,"margin-start", 3);
  return GTK_LABEL(label);
}

GtkGrid* tbl = NULL;
GtkCliboard* clip = NULL;

struct row {
  GtkButton* btn;
  GtkLabel* label;
  GtkLabel* counter;
  bool ready;
};
struct row* rows = NULL;
int nrows = 0;

// from D getContents(i) => const char*
static void on_click(GtkButton* button, gpointer udata) {
  int i = (int) udata;
  gtk_clipboard_set_text(clip,getContents(i),-1);
}

// D calls assureRow/refreshRow when it's reloaded its stuff
void assureRow(int i, const char* name) {
  struct row* row = g_new(struct row);
  if(nrows<=i) {
    nrows = i + 1;
    rows = realloc(rows,sizeof(struct row) * nrows);
  }
  struct row* cur = rows + i;
  if(row->ready) return;
  gtk_grid_insert_row(tbl,i+1);
  cur->btn = gtk_button_new_with_label(name);
  g_signal_connect("clicked",on_click,(void*)i);
  gtk_grid_attach(tbl,cur->btn,0,i+1,1,1);
  cur->label = makeLabel();
  gtk_grid_attach(tbl,cur->label,1,i+1,1,1);
  cur->counter = makeLabel();
  gtk_grid_attach(tbl,cur->counter,2,i+1,1,1);
  row->ready = true;
}
  
void refreshRow(int i, const char* summary, const char* count) {
  gtk_grid_insert_row(tbl,i+1);
  struct row* cur = rows + i;
  gtk_label_set_text(cur->label,summary);
  gtk_label_set_text(cur->counter, count);    
}

struct delegate {
  void* ctx;
  void (*funcptr)(void);
};

static void doRefresh(GtkButton* btn, void* udata) {
  struct delegate* dg = (struct delegate*) udata;
  reloadFile(dg->ctx,dg->funcptr); // causes D to call assureRow/refreshRow for each row.
}

static void
refreshPathChanged (GFileMonitor     *monitor,
                    GFile            *file,
                    GFile            *other_file,
                    GFileMonitorEvent event_type,
                    gpointer          udata) {
  if(event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
    //puts("updated");
    struct delegate* dg = (struct delegate*) udata;
    reloadFile(dg->ctx,dg->funcptr); // causes D to call assureRow/refreshRow for each row.
  }
}


void guiLoop(const char* path, void* ctx, void (*funcptr)(void)) {
  struct delegate dg = { ctx, funcptr };
  gtk_init(NULL,NULL);
  GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  tbl = gtk_grid_new();
  gtk_window_add(win,tbl);
  g_signal_connect(win,"destroy",gtk_main_quit);
  clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_grid_insert_row(tbl,0);
  GtkWidget* refreshbtn = gtk_button_new_with_label("Reload");
  gtk_grid_attach(tbl,refreshbtn,0,0,3,1);
  g_signal_connect(refreshbtn,"clicked",doRefresh,&dg);
  GFile* f = g_file_new_for_path(path);
  GError* err = NULL;
  GFileMonitor* mon = g_file_monitor_file
    (path,
     G_FILE_MONITOR_NONE,NULL,&err);
  assert(err==NULL);
  g_signal_connect(mon,"changed",refreshPathChanged,&dg);
  gtk_widget_show_all(win);
  gtk_main();
  exit(0);
}

