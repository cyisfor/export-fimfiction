#include <gtk/gtk.h>
#include <glib.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h> // realloc
#include <string.h> // memset

#include <assert.h>

extern void reloadFile(void*,void (*)(void));
extern const char* getContents(int);

static GtkLabel* makeLabel() {
  GtkLabel* label = GTK_LABEL(gtk_label_new("..."));
  gtk_label_set_justify(label,GTK_JUSTIFY_LEFT);
  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value,GTK_ALIGN_START);
  g_object_set_property(G_OBJECT(label),"halign",&value);
  g_value_set_int(&value,3);
    g_object_set_property(G_OBJECT(label),"margin-start", &value);
  return label;
}

GtkGrid* tbl = NULL;
GtkClipboard* clip = NULL;

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
  int i = (intptr_t) udata;
  gtk_clipboard_set_text(clip,getContents(i),-1);
}

// D calls refreshRow when it's reloaded its stuff
void refreshRow(int i, const char* name, const char* summary, const char* count) {
  int oldrows = nrows;
  while(nrows<=i) {
    ++nrows;
  }
  if(oldrows < nrows) {
    int j;
    rows = realloc(rows,sizeof(struct row) * nrows);
    for(j=oldrows;j<nrows;++j) {
      rows[j].ready = false; // XXX: this needs to be smarter
    }
  }
  struct row* cur = rows + i;
  if(!cur->ready) {
    memset(cur,0,sizeof(struct row));
    cur->btn = GTK_BUTTON(gtk_button_new_with_label(name));
    g_signal_connect(cur->btn,
                     "clicked",
                     G_CALLBACK(on_click),
                     (void*)(intptr_t)i);
    i = (i<<1) + 1;
    gtk_grid_insert_row(tbl,i);
    gtk_grid_attach(tbl,GTK_WIDGET(cur->btn),0,i,1,1);
    cur->label = makeLabel();
    gtk_grid_attach(tbl,GTK_WIDGET(cur->label),1,i,1,1);
    cur->counter = makeLabel();
    gtk_grid_attach(tbl,GTK_WIDGET(cur->counter),2,i,1,1);
    gtk_widget_show_all(GTK_WIDGET(tbl));
    cur->ready = true;
  }
  
  gtk_button_set_label(cur->btn, name);
  gtk_label_set_text(cur->label, summary);
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
  if(event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
    //puts("updated");
    struct delegate* dg = (struct delegate*) udata;
    reloadFile(dg->ctx,dg->funcptr); // causes D to call assureRow/refreshRow for each row.
  }
}


void guiLoop(const char* path, void* ctx, void (*funcptr)(void)) {
  struct delegate dg = { ctx, funcptr };
  gtk_init(NULL,NULL);
  GtkWidget* win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  tbl = GTK_GRID(gtk_grid_new());
  gtk_container_add(GTK_CONTAINER(win),GTK_WIDGET(tbl));
  g_signal_connect(win,"destroy",gtk_main_quit,NULL);
  clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_grid_insert_row(tbl,0);
  GtkWidget* refreshbtn = gtk_button_new_with_label("Reload");
  gtk_grid_attach(tbl,refreshbtn,0,0,3,1);
  g_signal_connect(refreshbtn,"clicked",G_CALLBACK(doRefresh),&dg);
  GFile* f = g_file_new_for_path(path);
  GError* err = NULL;
  GFileMonitor* mon = g_file_monitor_file
    (f,
     G_FILE_MONITOR_NONE,NULL,&err);
  assert(err==NULL);
  g_signal_connect(mon,"changed",G_CALLBACK(refreshPathChanged),&dg);
  gtk_widget_show_all(win);
  reloadFile(dg.ctx,dg.funcptr); // causes D to call 
  gtk_main();
  exit(0);
}

