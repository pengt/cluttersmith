
#ifndef CLUTTERSMITH_H
#define CLUTTERSMITH_H

/* Things that need to go in headers / get proper API: */
extern guint CS_REVISION;
extern guint CS_STORED_REVISION;

void actor_editing_init (gpointer stage);

extern ClutterActor *parasite_root;
extern ClutterActor *parasite_ui;

void set_title (const gchar *new_title);
void cluttersmith_set_active (ClutterActor *item);

void tree_populate (ClutterActor *scene_graph,
                    ClutterActor *active_actor);

/*
  returns in order, selected actor if it is a container otherwise the parent,
  if neither, look for an actor with id "actor" and ultimataly look at the
  stage. The stage to be worked on is determined by the actor passed in.*/
ClutterActor *cluttersmith_get_add_root (ClutterActor *actor);



/* actor-editing: */

/* selection */

GList   *cluttersmith_selected_get_list  (void);
gint     cluttersmith_selected_count     (void);
gboolean cluttersmith_selected_has_actor (ClutterActor *actor);
void     cluttersmith_selected_clear     (void);
void     cluttersmith_selected_init      (void);
void     cluttersmith_selected_add       (ClutterActor *actor);
void     cluttersmith_selected_remove    (ClutterActor *actor);
void     cluttersmith_selected_foreach   (GCallback     cb,
                                          gpointer      data);
gpointer cluttersmith_selected_match     (GCallback     match_fun,
                                          gpointer      data);
ClutterActor *cluttersmith_selected_get_any (void);


#endif
