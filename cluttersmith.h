
#ifndef CLUTTERSMITH_H
#define CLUTTERSMITH_H

/* Things that need to go in headers / get proper API: */
extern ClutterActor *active_actor;
extern guint CS_REVISION;
extern guint CS_STORED_REVISION;

void actor_editing_init (gpointer stage);

extern ClutterActor *parasite_root;
extern ClutterActor *parasite_ui;

void set_title (const gchar *new_title);
void select_item (ClutterActor *item);

/* actor-editing: */

extern GHashTable *selected;
GList *cluttersmith_get_selected (void);
void cluttersmith_selected_foreach (GCallback cb, gpointer data);
void cluttersmith_clear_selected (void);

void tree_populate (ClutterActor *scene_graph,
                    ClutterActor *active_actor);


#include "util.h"
#endif