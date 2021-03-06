/*
 * ClutterSmith - a visual authoring environment for clutter.
 * Copyright (c) 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * Alternatively, you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 3, with the following additional permissions:
 *
 * 1. Intel grants you an additional permission under Section 7 of the
 * GNU General Public License, version 3, exempting you from the
 * requirement in Section 6 of the GNU General Public License, version 3,
 * to accompany Corresponding Source with Installation Information for
 * the Program or any work based on the Program.  You are still required
 * to comply with all other Section 6 requirements to provide
 * Corresponding Source.
 *
 * 2. Intel grants you an additional permission under Section 7 of the
 * GNU General Public License, version 3, allowing you to convey the
 * Program or a work based on the Program in combination with or linked
 * to any works licensed under the GNU General Public License version 2,
 * with the terms and conditions of the GNU General Public License
 * version 2 applying to the combined or linked work as a whole.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Written by: Øyvind Kolås <oyvind.kolas@intel.com>
 */


#include <mx/mx.h>
#include <clutter/clutter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "cluttersmith.h"

/**** XXX: This serializer is a hack that will be replaced by
 * json-glib and clutterscript code
 */

static gboolean cs_filter_properties = TRUE;

#define INDENT {gint j;for (j=0;j<*indentation;j++) g_string_append_c (str, ' ');}

gchar *cs_json_escape_string (const gchar *in)
{
  GString *str = g_string_new ("");
  const gchar *p;
  gchar *ret;

  for (p=in;*p;p++)
    {
      if (*p=='"')
        g_string_append (str, "\\\"");
      else
        g_string_append_c (str, *p);
    }

  ret = str->str;
  g_string_free (str, FALSE);
  return ret;
}



static void
properties_to_string (GString      *str,
                      ClutterActor *actor,
                      gint         *indentation)
{
  GParamSpec **properties;
  GParamSpec **actor_properties;
  guint        n_properties;
  guint        n_actor_properties;
  gint         i;

  properties = g_object_class_list_properties (
                     G_OBJECT_GET_CLASS (actor),
                     &n_properties);
  actor_properties = g_object_class_list_properties (
                     G_OBJECT_GET_CLASS (actor),
                     &n_properties);
  actor_properties = g_object_class_list_properties (
            G_OBJECT_CLASS (g_type_class_ref (CLUTTER_TYPE_ACTOR)),
            &n_actor_properties);

  {
    const gchar *id = clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (actor));
    if (id && id[0]!='\0' && !g_str_has_prefix(id, "script-"))
      {
        g_string_append_printf (str, ",\n");
        INDENT;g_string_append_printf (str,"\"id\":\"%s\"", id);
      }
  }

  for (i = 0; i < n_properties; i++)
    {
      gint j;
      gboolean skip = FALSE;

      if (cs_filter_properties)
        {
          for (j=0;j<n_actor_properties;j++)
            {
              /* ClutterActor contains so many properties that we restrict our view a bit */
              if (actor_properties[j]==properties[i])
                {
                  gchar *whitelist[]={"x","y", "depth", "opacity", "width", "height",
                                      "scale-x","scale-y", "anchor-x", "color",
                                      "anchor-y", "rotation-angle-z", "reactive",
                                      "name", "rotation-angle-y", "rotation-angle-x",
                                      NULL};
                  gint k;
                  skip = TRUE;
                  for (k=0;whitelist[k];k++)
                    if (g_str_equal (properties[i]->name, whitelist[k]))
                      skip = FALSE;
                }
            }
        }

      if (! ( (properties[i]->flags & G_PARAM_READABLE) &&
              (properties[i]->flags & G_PARAM_WRITABLE)
          ))
        skip = TRUE;

      if (skip)
        continue;

      {
        if (properties[i]->value_type == G_TYPE_FLOAT)
          {
            gfloat value;
            g_object_get (actor, properties[i]->name, &value, NULL);

            if (value == G_PARAM_SPEC_FLOAT (properties[i])->default_value)
              continue;

            if (g_str_equal (properties[i]->name, "x")||
                g_str_equal (properties[i]->name, "y")||
                g_str_equal (properties[i]->name, "width")||
                g_str_equal (properties[i]->name, "height"))
              {
                /* XXX: clutter fails to read it back in without truncation */
                g_string_append_printf (str, ",\n");
                INDENT;g_string_append_printf (str,"\"%s\":%0.0f",
                                        properties[i]->name, value);
              }
            else
              {
                g_string_append_printf (str, ",\n");
                INDENT;g_string_append_printf (str,"\"%s\":%0.3f",
                                        properties[i]->name, value);
              }
          }
        else if (properties[i]->value_type == G_TYPE_DOUBLE)
          {
            gdouble value;
            g_object_get (actor, properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_DOUBLE (properties[i])->default_value)
              continue;

            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":%0.3f",
                                           properties[i]->name, value);
          }
        else if (properties[i]->value_type == G_TYPE_UCHAR)
          {
            guchar value;
            g_object_get (actor, properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_UCHAR (properties[i])->default_value)
              continue;
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":%i",
                                           properties[i]->name, value);
          }
        else if (properties[i]->value_type == G_TYPE_INT)
          {
            gint value;
            g_object_get (actor, properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_INT (properties[i])->default_value)
              continue;
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":%i",
                                           properties[i]->name, value);
          }
        else if (properties[i]->value_type == G_TYPE_UINT)
          {
            guint value;
            g_object_get (actor, properties[i]->name, &value, NULL);
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":%u",
                                           properties[i]->name, value);
          }
        else if (properties[i]->value_type == G_TYPE_STRING)
          {
            gchar *value;
            g_object_get (actor, properties[i]->name, &value, NULL);

            /* Make ClutterTextures used relative when saving */
            if (CLUTTER_IS_TEXTURE (actor) &&
                g_str_equal (properties[i]->name, "filename") &&
                g_path_is_absolute (value))
              {
                if (g_str_has_prefix (value, cs_get_project_root ()))
                  {
                    gchar *oldvalue = value;
                    value = strdup ((value + strlen (cs_get_project_root ()) + 1));
                    g_free (oldvalue);
                  }
                else
                  {
                    g_warning ("Asset %s not under project root\n", value);
                  }
              }

            if (value)
              {
                gchar *escaped;
                if (!g_strcmp0 (value, G_PARAM_SPEC_STRING (properties[i])->default_value))
                  continue;
                escaped  = cs_json_escape_string (value);
                g_string_append_printf (str, ",\n");
                INDENT;g_string_append_printf (str,"\"%s\":\"%s\"",
                                               properties[i]->name, escaped);
                g_free (escaped);
                g_free (value);
              }
          }
        else if (properties[i]->value_type == G_TYPE_BOOLEAN)
          {
            gboolean value;
            g_object_get (actor, properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_BOOLEAN (properties[i])->default_value)
              continue;
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":%s",
                                           properties[i]->name, value?"true":"false");
          }
        else if (properties[i]->value_type == CLUTTER_TYPE_COLOR)
          {
            GValue value = {0,};
            GValue str_value = {0,};
            g_value_init (&value, properties[i]->value_type);
            g_value_init (&str_value, G_TYPE_STRING);
            g_object_get_property (G_OBJECT (actor), properties[i]->name, &value);
            if (g_value_transform (&value, &str_value))
              {
                g_string_append_printf (str, ",\n");
                INDENT;g_string_append_printf (str,"\"%s\":\"%s\"",
                                               properties[i]->name, g_value_get_string (&str_value));
              }
            else
              {
              }
          }
        else if (g_type_is_a (properties[i]->value_type, G_TYPE_ENUM))
          {
            gboolean    value;
            GEnumClass *enum_class;
            GEnumValue *enum_value;

            enum_class = g_type_class_peek (properties[i]->value_type);
            g_object_get (actor, properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_ENUM (properties[i])->default_value)
              continue;
            enum_value = g_enum_get_value (enum_class, value);
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":\"%s\"",
                                           properties[i]->name, enum_value->value_nick);
          }
        else
          {
#if 0
            GValue value = {0,};
            GValue str_value = {0,};
            gchar *initial;
            g_value_init (&value, properties[i]->value_type);
            g_value_init (&str_value, G_TYPE_STRING);
            g_object_get_property (G_OBJECT (actor), properties[i]->name, &value);
            if (g_value_transform (&value, &str_value))
              {
                INDENT;g_string_append_printf (str,"\"%s\":%s,\n",
                                               properties[i]->name, g_value_get_string (&str_value));
              }
            else
              {
              }
#endif
          }
      }
    }
  
  {
    const gchar *name = clutter_actor_get_name (actor);
    if (name && g_str_has_prefix (name, "json-extra:"))
      {
        INDENT; g_string_append_printf (str, name+11);
        g_string_append_printf (str, "\n");
      }
  }
  g_free (properties);


  /* should be split into its own function */
  if (CLUTTER_IS_ACTOR (actor))
  {
    ClutterActor *parent;
    parent = clutter_actor_get_parent (actor);

    if (parent && CLUTTER_IS_CONTAINER (parent))
      {
        ClutterChildMeta *child_meta;
        GParamSpec **child_properties = NULL;
        guint        n_child_properties=0;
        child_meta = clutter_container_get_child_meta (CLUTTER_CONTAINER (parent), actor);
        if (child_meta)
          {
            child_properties = g_object_class_list_properties (
                               G_OBJECT_GET_CLASS (child_meta),
                               &n_child_properties);
            for (i = 0; i < n_child_properties; i++)
              {
                if (!G_TYPE_IS_OBJECT (child_properties[i]->value_type) &&
                    child_properties[i]->value_type != CLUTTER_TYPE_CONTAINER)
                  {
      gboolean skip = FALSE;
#if 0
      gint j;
      if (cs_filter_properties)
        {
          for (j=0;j<n_actor_properties;j++)
            {
              /* ClutterActor contains so many properties that we restrict our view a bit */
              if (actor_properties[j]==properties[i])
                {
                  gchar *whitelist[]={"x","y", "depth", "opacity", "width", "height",
                                      "scale-x","scale-y", "anchor-x", "color",
                                      "anchor-y", "rotation-angle-z",
                                      "name", 
                                      NULL};
                  gint k;
                  skip = TRUE;
                  for (k=0;whitelist[k];k++)
                    if (g_str_equal (properties[i]->name, whitelist[k]))
                      skip = FALSE;
                }
            }
        }
#endif

      if (! ( (child_properties[i]->flags & G_PARAM_READABLE) &&
              (child_properties[i]->flags & G_PARAM_WRITABLE)
          ))
        skip = TRUE;

      if (skip)
        continue;

      {
        if (child_properties[i]->value_type == G_TYPE_FLOAT)
          {
            gfloat value;
            /* XXX: clutter fails to read it back in without truncation */
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_FLOAT (child_properties[i])->default_value)
              continue;
            {
              g_string_append_printf (str, ",\n");
              INDENT;g_string_append_printf (str,"\"child::%s\":%0.3f",
                                             child_properties[i]->name, value);
            }
          }
        else if (child_properties[i]->value_type == G_TYPE_DOUBLE)
          {
            gdouble value;
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_DOUBLE (child_properties[i])->default_value)
              continue;
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"child::%s\":%0.3f",
                                           child_properties[i]->name, value);
          }
        else if (child_properties[i]->value_type == G_TYPE_UCHAR)
          {
            guchar value;
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_UCHAR (child_properties[i])->default_value)
              continue;
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"child::%s\":%i",
                                           child_properties[i]->name, value);
          }
        else if (child_properties[i]->value_type == G_TYPE_INT)
          {
            gint value;
            g_string_append_printf (str, ",\n");
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            INDENT;g_string_append_printf (str,"\"child::%s\":%i",
                                           child_properties[i]->name, value);
          }
        else if (child_properties[i]->value_type == G_TYPE_UINT)
          {
            guint value;
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"child::%s\":%u",
                                           child_properties[i]->name, value);
          }
        else if (child_properties[i]->value_type == G_TYPE_STRING)
          {
            gchar *value;
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            if (!g_strcmp0 (value, G_PARAM_SPEC_STRING (properties[i])->default_value))
                  continue;
            if (value)
              {
                gchar *escaped = cs_json_escape_string (value);
                g_string_append_printf (str, ",\n");
                INDENT;g_string_append_printf (str,"\"child::%s\":\"%s\"",
                                               child_properties[i]->name, escaped);
                g_free (escaped);
                g_free (value);
              }
          }
        else if (child_properties[i]->value_type == G_TYPE_BOOLEAN)
          {
            gboolean value;
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_BOOLEAN (child_properties[i])->default_value)
              continue;
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"child::%s\":%s",
                                           child_properties[i]->name, value?"true":"false");
          }
        else if (child_properties[i]->value_type == CLUTTER_TYPE_COLOR)
          {
            GValue value = {0,};
            GValue str_value = {0,};
            g_value_init (&value, child_properties[i]->value_type);
            g_value_init (&str_value, G_TYPE_STRING);
            g_object_get_property (G_OBJECT (child_meta), child_properties[i]->name, &value);
            if (g_value_transform (&value, &str_value))
              {
                g_string_append_printf (str, ",\n");
                INDENT;g_string_append_printf (str,"\"child::%s\":\"%s\"",
                                               child_properties[i]->name, g_value_get_string (&str_value));
              }
            else
              {
              }
          }
        else if (g_type_is_a (child_properties[i]->value_type, G_TYPE_ENUM))
          {
            gboolean    value;
            GEnumClass *enum_class;
            GEnumValue *enum_value;

            enum_class = g_type_class_peek (child_properties[i]->value_type);
            g_object_get (child_meta, child_properties[i]->name, &value, NULL);
            if (value == G_PARAM_SPEC_ENUM (child_properties[i])->default_value)
              continue;
            enum_value = g_enum_get_value (enum_class, value);
            g_string_append_printf (str, ",\n");
            INDENT;g_string_append_printf (str,"\"%s\":\"%s\"",
                                           child_properties[i]->name, enum_value->value_nick);
          }
        else
          {
#if 0
            GValue value = {0,};
            GValue str_value = {0,};
            gchar *initial;
            g_value_init (&value, child_properties[i]->value_type);
            g_value_init (&str_value, G_TYPE_STRING);
            g_object_get_property (G_OBJECT (child_meta), child_properties[i]->name, &value);
            if (g_value_transform (&value, &str_value))
              {
                INDENT;g_string_append_printf (str,"\"child::%s\":%s,\n",
                                               child_properties[i]->name, g_value_get_string (&str_value));
              }
            else
              {
              }
#endif
          }
      }

                  }
              }
            g_free (child_properties);
          }
      }
  }
}


static void
actor_to_string (GString      *str,
                 ClutterActor *iter,
                 gint         *indentation)
{

  if (iter == NULL ||
      cs_actor_has_ancestor (iter, cs->parasite_root))
    {
      return;
    }

  INDENT;
  g_string_append_printf (str, "{\n");
  *indentation+=2;
  INDENT;

  if (CLUTTER_IS_STAGE (iter))
    {
      g_print ("DUMPING stage!\n");
      g_string_append_printf (str, "\"id\":\"actor\",\n");
      g_string_append_printf (str, "\"type\":\"ClutterGroup\"\n");
    }
  else
    {
      g_string_append_printf (str, "\"type\":\"%s\"", G_OBJECT_TYPE_NAME (iter));
      properties_to_string (str, iter, indentation);
    }

  if (CLUTTER_IS_CONTAINER (iter))
    {
      GList *children, *c;
      children = clutter_container_get_children (CLUTTER_CONTAINER (iter));

      g_string_append_printf (str, ",\n");
      INDENT;
      g_string_append_printf (str, "\"children\":\[\n");
      *indentation+=2;
      for (c = children; c; c=c->next)
        {
          actor_to_string (str, c->data, indentation);
          if (c->next)
            {
              INDENT;
              g_string_append_printf (str, ",\n");
            }
        }
      *indentation-=2;
      INDENT;
      g_string_append_printf (str, "]");
      g_list_free (children);
    }
  *indentation-=2;
  g_string_append_printf (str, "\n");
  INDENT;
  g_string_append_printf (str, "}\n");
}

gchar *json_serialize_subtree (ClutterActor *root)
{
  GString *str = g_string_new ("");
  gchar   *ret;
  gint     indentation = 0;
  actor_to_string (str, root, &indentation);
  ret = str->str;
  g_string_free (str, FALSE);
  return ret;
}


static void
animator_to_string (GString         *str,
                    ClutterAnimator *animator,
                    gint            *indentation)
{
  GList *keys = clutter_animator_get_keys (animator, NULL, NULL, -1.0);
  INDENT;
  g_string_append_printf (str, "{\n");
  *indentation+=2;
  INDENT;
  g_string_append_printf (str, "\"id\":\"%s\",\n", clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (animator)));
  INDENT;
  g_string_append_printf (str, "\"type\":\"ClutterAnimator\",\n");
  INDENT;
  g_string_append_printf (str, "\"duration\":%i,\n", clutter_animator_get_duration (animator));
  INDENT;
  g_string_append_printf (str, "\"properties\":[\n");
  
  *indentation+=2;
  {
    GList *iter;
    const gchar *curprop = NULL;
    GObject *curobject = NULL;
    gboolean gotprop = FALSE;

    for (iter = keys; iter; iter=iter->next)
      {
        ClutterAnimatorKey *key = iter->data;
        const gchar *prop     = clutter_animator_key_get_property_name (key);
        GObject     *object   = clutter_animator_key_get_object (key);
        gdouble      progress = clutter_animator_key_get_progress (key);
        guint        mode     = clutter_animator_key_get_mode (key);


        if (object != curobject ||
            prop != curprop)
          {
            if (curobject == NULL)
              {
                INDENT;
                g_string_append_printf (str, "{\n");
                *indentation+=2;
              }
            else
              {
                *indentation-=2;
                INDENT;
                g_string_append_printf (str, "]\n");
                *indentation-=2;
                INDENT;
                g_string_append_printf (str, "},{\n");
                *indentation+=2;
              }
            INDENT;
            g_string_append_printf (str, "\"object\":\"%s\",\n", clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (object)));
            INDENT;
            g_string_append_printf (str, "\"name\":\"%s\",\n", prop);
            INDENT;
            g_string_append_printf (str, "\"ease-in\":%s,\n",
              clutter_animator_property_get_ease_in (animator, object, prop)?"true":"false");
            INDENT;
            g_string_append_printf (str, "\"interpolation\":\"%s\",\n",
              clutter_animator_property_get_interpolation (animator, object, prop) ==
              CLUTTER_INTERPOLATION_LINEAR?"linear":"cubic");
            INDENT;
            g_string_append_printf (str, "\"keys\": [\n");
            gotprop = FALSE;
            *indentation+=2;
          }
        INDENT;

        {
          GValue value = {0,};
          GEnumClass *enum_class;
          GEnumValue *enum_value;

          mode = clutter_animator_key_get_mode (key);
          enum_class = g_type_class_peek (CLUTTER_TYPE_ANIMATION_MODE);
          enum_value = g_enum_get_value (enum_class, mode);
          g_value_init (&value, G_TYPE_STRING); /* XXX: this isnt very robust */
          clutter_animator_key_get_value (key, &value);
          g_string_append_printf (str, "%s[%f, \"%s\", %s]\n",
                                  gotprop?",":" ", progress,
                                  enum_value->value_nick,
                                  g_value_get_string (&value));
          g_value_unset (&value);
        }
        gotprop=TRUE;
        
        curobject = object;
        curprop = prop;
      }
    if (curobject)
      {
        *indentation-=2;
      }
  }
  INDENT;
  g_string_append_printf (str, "]\n");
  *indentation-=2;

  INDENT;
  g_string_append_printf (str, "}\n");
  *indentation-=2;

  INDENT;
  g_string_append_printf (str, "]\n");

  *indentation-=2;
  INDENT;
  g_string_append_printf (str, "}\n");

  g_list_free (keys);
}

gchar *json_serialize_animator (ClutterAnimator *animator)
{
  GString *str = g_string_new ("");
  gchar   *ret;
  gint     indentation = 0;
  animator_to_string (str, animator, &indentation);
  ret = str->str;
  g_string_free (str, FALSE);
  return ret;
}

static gint
sort_transitions_func (gconstpointer a,
                       gconstpointer b)
{
  const ClutterStateKey *ka = a;
  const ClutterStateKey *kb = b;

  if (clutter_state_key_get_target_state_name (ka) !=
      clutter_state_key_get_target_state_name (kb))
    return (clutter_state_key_get_target_state_name (ka) -
                   clutter_state_key_get_target_state_name (kb));

  return (clutter_state_key_get_source_state_name (ka) -
          clutter_state_key_get_source_state_name (kb));
}

static void
state_to_string (GString      *str,
                 ClutterState *state,
                 gint         *indentation)
{
  GList *transitions = clutter_state_get_keys (state, NULL, NULL, NULL, NULL);
  INDENT;
  g_string_append_printf (str, "{\n");
  *indentation+=2;
  INDENT;
  g_string_append_printf (str, "\"type\":\"ClutterState\",\n");
  INDENT;
  g_string_append_printf (str, "\"id\":\"%s\",\n", clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (state)));
  INDENT;
  g_string_append_printf (str, "\"duration\":%d,\n", clutter_state_get_duration (state, NULL, NULL));
  INDENT;
  g_string_append_printf (str, "\"transitions\":[\n");
  *indentation+=2;
  *indentation+=2;

  transitions = g_list_sort (transitions, sort_transitions_func);

  {
    GList *iter;
    const gchar *curr_sstate = NULL;
    const gchar *curr_tstate = NULL;
    gboolean had_prev = FALSE;
    gboolean first_key = TRUE;

    for (iter = transitions; iter; iter=iter->next)
      {
        ClutterStateKey *key = iter->data;
        const gchar *prop         = clutter_state_key_get_property_name (key);
        GObject     *object       = clutter_state_key_get_object (key);
        const gchar *source_state = clutter_state_key_get_source_state_name (key);
        const gchar *target_state = clutter_state_key_get_target_state_name (key);
        guint        mode         = clutter_state_key_get_mode (key);
        gdouble      pre_delay    = clutter_state_key_get_pre_delay (key);
        gdouble      post_delay   = clutter_state_key_get_post_delay (key);

        if (curr_sstate != source_state ||
            curr_tstate != target_state)
          {
            ClutterAnimator *animator;
            guint            duration;

            animator =
               clutter_state_get_animator (state, source_state, target_state);
            duration =
               clutter_state_get_duration (state, source_state, target_state);

            curr_sstate = source_state;
            curr_tstate = target_state;
            if (had_prev)
              {
                INDENT
                g_string_append_printf (str, "]\n");
                *indentation-=2;
                INDENT
                g_string_append_printf (str, "},{\n");
              }
            else
              {
                INDENT
                g_string_append_printf (str, "{\n");
              }
            *indentation+=2;

            if (source_state)
              {
                INDENT;g_string_append_printf (str, "\"source\":\"%s\",\n",
                                               source_state);
              }
            else
              {
                INDENT;g_string_append_printf (str, "\"source\":null,\n");
              }

            if (target_state)
              {
                INDENT;g_string_append_printf (str, "\"target\":\"%s\",\n",
                                               target_state);
              }
            else
              {
                INDENT;g_string_append_printf (str, "\"target\":null,\n");
              }

            if (animator)
              {
                INDENT;
                g_string_append_printf (str, "\"animator\":\"%s\",\n",
                      clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (animator)));
              }
            if (clutter_state_get_duration (state, NULL, NULL) != duration)
              {
                INDENT;g_string_append_printf (str, "\"duration\":%d,\n", duration);
              }
            INDENT;g_string_append_printf (str, "\"keys\": [\n");

            first_key = TRUE;
          }

        INDENT
        if (!first_key)
          g_string_append_printf (str, ",");

        g_string_append_printf (str, "[\"%s\", \"%s\",  \"%s\", ",
                   clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (object)),
                   prop, g_enum_get_value (g_type_class_ref (CLUTTER_TYPE_ANIMATION_MODE), mode)->value_nick);

        {
          GValue value = {0,};

          g_value_init (&value, G_TYPE_STRING); /* XXX: this isnt very robust */
          clutter_state_key_get_value (key, &value);
          g_string_append_printf (str, "%s, ", g_value_get_string (&value));
          g_value_unset (&value);
        }

        g_string_append_printf (str, "%f, %f]\n", pre_delay, post_delay);


        had_prev = TRUE;
        first_key = FALSE;
      }
    INDENT
    g_string_append_printf (str, "]\n");
    *indentation-=2;
    INDENT
    g_string_append_printf (str, "}\n");
  }

  *indentation-=2;
  INDENT;
  g_string_append_printf (str, "]\n");
  *indentation-=2;
  INDENT;
  g_string_append_printf (str, "}\n");
}

gchar *json_serialize_state (ClutterState *state)
{
  GString *str = g_string_new ("");
  gchar   *ret;
  gint     indentation = 0;
  state_to_string (str, state, &indentation);
  ret = str->str;
  g_string_free (str, FALSE);
  return ret;
}
