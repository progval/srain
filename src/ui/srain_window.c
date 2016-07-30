/**
 * @file srain_window.c
 * @brief Srain's main windows
 * @author Shengyu Zhang <lastavengers@outlook.com>
 * @version 1.0
 * @date 2016-03-01
 */

#define __LOG_ON

#include <gtk/gtk.h>
#include <string.h>
#include <assert.h>

#include "ui_common.h"
#include "ui_hdr.h"
#include "theme.h"
#include "srain_app.h"
#include "srain_window.h"
#include "srain_chan.h"
#include "srain_stack_sidebar.h"
#include "tray_icon.h"

#include "meta.h"
#include "log.h"
#include "i18n.h"

typedef struct {
    // join_popover
    GtkEntry *join_chan_entry;
    GtkEntry *join_pwd_entry;
} JoinEntries;

typedef struct {
    // conn_popover
    GtkEntry *conn_addr_entry;
    GtkEntry *conn_port_entry;
    GtkEntry *conn_pwd_entry;
    GtkEntry *conn_nick_entry;
    GtkEntry *conn_real_entry;
} ConnEntries;

struct _SrainWindow {
    GtkApplicationWindow parent;

    // Header
    GtkButton *about_button;
    GtkButton *conn_popover_button;
    GtkButton *join_popover_button;
    GtkSpinner *spinner;

    GtkBox *sidebar_box;
    SrainStackSidebar *sidebar;
    GtkStack *stack;
    GtkStatusIcon *tray_icon;
    GtkMenu *tray_menu;
    GtkMenuItem *quit_menu_item;

    // join_popover
    GtkPopover *join_popover;
    GtkEntry *join_chan_entry;
    GtkEntry *join_pwd_entry;
    GtkButton *join_button;
    JoinEntries join_entries;

    // conn_popover
    GtkPopover *conn_popover;
    GtkEntry *conn_addr_entry;
    GtkEntry *conn_port_entry;
    GtkEntry *conn_pwd_entry;
    GtkEntry *conn_nick_entry;
    GtkEntry *conn_real_entry;
    GtkButton *conn_button;
    ConnEntries conn_entries;
};

struct _SrainWindowClass {
    GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(SrainWindow, srain_window, GTK_TYPE_APPLICATION_WINDOW);

static void quit_menu_item_on_activate(){
    srain_app_quit(srain_app);
}

static void about_button_on_click(gpointer user_data){
    GtkWidget *window = user_data;
    const gchar *authors[] = { PACKAGE_AUTHOR " <" PACKAGE_EMAIL ">", NULL };
    const gchar **documentors = authors;
    const gchar *version = g_strdup_printf(_("%s\nRunning against GTK+ %d.%d.%d"),
            PACKAGE_VERSION,
            gtk_get_major_version(),
            gtk_get_minor_version(),
            gtk_get_micro_version());

    gtk_show_about_dialog(GTK_WINDOW(window),
            "program-name", PACKAGE_NAME,
            "version", version,
            "copyright", "(C) 2016 " PACKAGE_AUTHOR,
            "license-type", GTK_LICENSE_GPL_3_0,
            "website", PACKAGE_WEBSITE,
            "comments", PACKAGE_DESC,
            "authors", authors,
            "documenters", documentors,
            "logo-icon-name", "gtk3-demo",
            "title", _("About Srain"),
            NULL);
}

static void popover_button_on_click(gpointer user_data){
    GtkPopover *popover;

    popover = user_data;
    gtk_widget_set_visible(GTK_WIDGET(popover),
            !gtk_widget_get_visible(GTK_WIDGET(popover)));
}

static void popover_entry_on_activate(GtkWidget *widget, gpointer user_data){
    GtkWidget *parent;
    GtkEntry *entry;
    GtkButton *button;

    entry = GTK_ENTRY(widget);
    button = user_data;

    parent = gtk_widget_get_parent(GTK_WIDGET(entry));

    // Move focus to next entry, if reach the last one, "click" the button
    if (!gtk_widget_child_focus(GTK_WIDGET(parent), GTK_DIR_TAB_FORWARD))
        g_signal_emit_by_name(button, "clicked");
}

static void join_button_on_click(gpointer user_data){
    const char *chan;
    const char *pwd;
    GString *cmd;
    JoinEntries *join_entries;

    join_entries = user_data;

    chan = gtk_entry_get_text(join_entries->join_chan_entry);
    pwd = gtk_entry_get_text(join_entries->join_pwd_entry);

    cmd = g_string_new("");
    g_string_printf(cmd, "/join %s %s", chan, pwd);
    ui_hdr_srv_cmd(srain_window_get_cur_chan(srain_win), cmd->str);
    g_string_free(cmd, TRUE);

    gtk_entry_set_text(join_entries->join_chan_entry, "");
    gtk_entry_set_text(join_entries->join_pwd_entry, "");

}

static void conn_button_on_click(gpointer user_data){
    const char *addr;
    const char *port;
    const char *passwd;
    const char *nick;
    const char *realname;
    GString *cmd;
    ConnEntries *conn_entries;

    conn_entries = user_data;

    addr = gtk_entry_get_text(conn_entries->conn_addr_entry);
    port = gtk_entry_get_text(conn_entries->conn_port_entry);
    passwd = gtk_entry_get_text(conn_entries->conn_pwd_entry);
    nick = gtk_entry_get_text(conn_entries->conn_nick_entry);
    realname = gtk_entry_get_text(conn_entries->conn_real_entry);

    cmd = g_string_new("");

    g_string_printf(cmd, "/connect %s %s", addr, nick);
    if (strlen(port) > 0) g_string_append_printf(cmd, " port=%s", port);
    if (strlen(passwd) > 0) g_string_append_printf(cmd, " passwd=%s", passwd);
    if (strlen(realname) > 0) g_string_append_printf(cmd, " realname=%s", realname);

    ui_hdr_srv_cmd(srain_window_get_cur_chan(srain_win), cmd->str);

    g_string_free(cmd, TRUE);

    gtk_entry_set_text(conn_entries->conn_addr_entry, "");
    gtk_entry_set_text(conn_entries->conn_port_entry, "");
    gtk_entry_set_text(conn_entries->conn_pwd_entry, "");
    gtk_entry_set_text(conn_entries->conn_nick_entry, "");
    gtk_entry_set_text(conn_entries->conn_real_entry, "");

    // TODO: Send command to server
}

static gboolean CTRL_J_K_on_press(GtkAccelGroup *group, GObject *obj,
        guint keyval, GdkModifierType mod, gpointer user_data){
    SrainStackSidebar *sidebar;

    if (mod != GDK_CONTROL_MASK) return FALSE;

    sidebar = user_data;
    switch (keyval){
        case GDK_KEY_k:
            LOG_FR("<C-k>");
            srain_stack_sidebar_prev(sidebar);
            break;
        case GDK_KEY_j:
            LOG_FR("<C-j>");
            srain_stack_sidebar_next(sidebar);
            break;
        default:
            ERR_FR("unknown keyval %d", keyval);
            return FALSE;
    }

    return TRUE;
}

static void srain_window_init(SrainWindow *self){
    GClosure *closure_j;
    GClosure *closure_k;
    GtkAccelGroup *accel;

    gtk_widget_init_template(GTK_WIDGET(self));

    // Filling entries
    self->join_entries.join_chan_entry = self->join_chan_entry;
    self->join_entries.join_pwd_entry = self->join_pwd_entry;
    self->conn_entries.conn_addr_entry = self->conn_addr_entry;
    self->conn_entries.conn_port_entry = self->conn_port_entry;
    self->conn_entries.conn_pwd_entry = self->conn_pwd_entry;
    self->conn_entries.conn_nick_entry = self->conn_nick_entry;
    self->conn_entries.conn_real_entry = self->conn_real_entry;

    /* stack sidebar init */
    self->sidebar = srain_stack_sidebar_new();
    gtk_widget_show(GTK_WIDGET(self->sidebar));
    gtk_box_pack_start(self->sidebar_box, GTK_WIDGET(self->sidebar),
            TRUE, TRUE, 0);
    srain_stack_sidebar_set_stack(self->sidebar, self->stack);

    theme_apply(GTK_WIDGET(self));
    theme_apply(GTK_WIDGET(self->tray_menu));

    tray_icon_set_callback(self->tray_icon, self, self->tray_menu);
    g_signal_connect_swapped(self->quit_menu_item, "activate",
            G_CALLBACK(quit_menu_item_on_activate), NULL);

    // Click to show/hide GtkPopover
    g_signal_connect_swapped(self->about_button, "clicked",
            G_CALLBACK(about_button_on_click), self);
    g_signal_connect_swapped(self->join_popover_button, "clicked",
            G_CALLBACK(popover_button_on_click), self->join_popover);
    g_signal_connect_swapped(self->conn_popover_button, "clicked",
            G_CALLBACK(popover_button_on_click), self->conn_popover);
    g_signal_connect_swapped(self->join_button, "clicked",
            G_CALLBACK(popover_button_on_click), self->join_popover);
    g_signal_connect_swapped(self->conn_button, "clicked",
            G_CALLBACK(popover_button_on_click), self->conn_popover);

    // Click to submit entries' messages
    g_signal_connect_swapped(self->join_button, "clicked",
            G_CALLBACK(join_button_on_click), &(self->join_entries));
    g_signal_connect_swapped(self->conn_button, "clicked",
            G_CALLBACK(conn_button_on_click), &(self->conn_entries));

    // Press ENTER to move focus or submit entries' messages
    g_signal_connect(self->join_chan_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->join_button);
    g_signal_connect(self->join_pwd_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->join_button);
    g_signal_connect(self->conn_addr_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->conn_button);
    g_signal_connect(self->conn_port_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->conn_button);
    g_signal_connect(self->conn_pwd_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->conn_button);
    g_signal_connect(self->conn_nick_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->conn_button);
    g_signal_connect(self->conn_real_entry, "activate",
            G_CALLBACK(popover_entry_on_activate), self->conn_button);

    /* shortcut <C-j> and <C-k> */
    accel = gtk_accel_group_new();

    closure_j = g_cclosure_new(G_CALLBACK(CTRL_J_K_on_press),
            self->sidebar, NULL);
    closure_k = g_cclosure_new(G_CALLBACK(CTRL_J_K_on_press),
            self->sidebar, NULL);

    gtk_accel_group_connect(accel, GDK_KEY_j, GDK_CONTROL_MASK,
            GTK_ACCEL_VISIBLE, closure_j);
    gtk_accel_group_connect(accel, GDK_KEY_k, GDK_CONTROL_MASK,
            GTK_ACCEL_VISIBLE, closure_k);

    gtk_window_add_accel_group(GTK_WINDOW(self), accel);

    g_closure_unref(closure_j);
    g_closure_unref(closure_k);
}

static void srain_window_class_init(SrainWindowClass *class){
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
            "/org/gtk/srain/window.glade");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, about_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, join_popover_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_popover_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, spinner);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, stack);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, tray_icon);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, tray_menu);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, quit_menu_item);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, sidebar_box);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, join_popover);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_popover);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, join_chan_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, join_pwd_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, join_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_addr_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_port_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_pwd_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_nick_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_real_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), SrainWindow, conn_button);
}

SrainWindow* srain_window_new(SrainApp *app){
    return g_object_new(SRAIN_TYPE_WINDOW, "application", app, NULL);
}

SrainChan* srain_window_add_chan(SrainWindow *win,
        const char *server_name, const char *chan_name){
    SrainChan *chan;

    if (srain_window_get_chan_by_name(win, server_name, chan_name)){
        ERR_FR("server_name: %s, chan_name: %s, already exist",
                server_name, chan_name);
        return NULL;
    }

    chan = srain_chan_new(server_name, chan_name);

    GString *gstr = g_string_new("");
    g_string_printf(gstr, "%s %s", server_name, chan_name);
    gtk_stack_add_named(win->stack, GTK_WIDGET(chan), gstr->str);
    g_string_free(gstr, TRUE);

    theme_apply(GTK_WIDGET(win));

    gtk_stack_set_visible_child (win->stack, GTK_WIDGET(chan));
    return chan;
}

void srain_window_rm_chan(SrainWindow *win, SrainChan *chan){
    char *chan_name;
    char *server_name;

    chan_name = g_object_get_data(G_OBJECT(chan), "chan-name");
    server_name = g_object_get_data(G_OBJECT(chan), "server-name");
    free(chan_name);
    free(server_name);

    srain_user_list_clear(srain_chan_get_user_list(chan));
    gtk_container_remove(GTK_CONTAINER(win->stack), GTK_WIDGET(chan));
}

SrainChan* srain_window_get_cur_chan(SrainWindow *win){
    SrainChan *chan = NULL;

    chan = SRAIN_CHAN(gtk_stack_get_visible_child(win->stack));

    // TODO:
    //  if (chan == NULL) ERR_FR("no visible chan");

    return chan;
}

SrainChan* srain_window_get_chan_by_name(SrainWindow *win,
        const char *server_name, const char *chan_name){
    SrainChan *chan = NULL;

    GString *name = g_string_new("");
    g_string_printf(name, "%s %s", server_name, chan_name);
    chan = SRAIN_CHAN(gtk_stack_get_child_by_name(win->stack, name->str));
    g_string_free(name, TRUE);

    return chan;
}

/**
 * @brief Find out all SrainChans with the server_name given as argument
 *
 * @param win
 * @param server_name
 *
 * @return a GList, may be NULL, should be freed by caller
 */
GList* srain_window_get_chans_by_srv_name(SrainWindow *win,
        const char *server_name){
    GList *all_chans;
    GList *chans = NULL;
    SrainChan *chan = NULL;

    all_chans = gtk_container_get_children(GTK_CONTAINER(win->stack));
    while (all_chans){
        chan = SRAIN_CHAN(all_chans->data);

        if (strncmp(server_name,
                    srain_chan_get_server_name(chan),
                    strlen(server_name)) == 0){
            chans = g_list_append(chans, chan);
        }
        all_chans = g_list_next(all_chans);
    }

    return chans;
}

void srain_window_spinner_toggle(SrainWindow *win, gboolean is_busy){
   is_busy
        ? gtk_spinner_start(win->spinner)
        : gtk_spinner_stop(win->spinner);
}

void srain_window_stack_sidebar_update(SrainWindow *win, SrainChan *chan,
        const char *nick, const char *msg){
    if (SRAIN_CHAN(gtk_stack_get_visible_child(win->stack)) != chan){
        srain_stack_sidebar_update(win->sidebar, chan, nick, msg, 0);
    } else {
        srain_stack_sidebar_update(win->sidebar, chan, nick, msg, 1);
    }
}
