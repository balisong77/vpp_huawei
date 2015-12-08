/*
 * Copyright (c) 2015 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <vnet/vnet.h>

#define vl_api_version(n,v) static u32 vpe_api_version = (v);
#include <api/vpe.api.h>
#undef vl_api_version

#include <jni.h>
#include <japi/vppjni.h>
#include <japi/vppjni_bridge_domain.h>
#include <japi/org_openvpp_vppjapi_vppConn.h>
#include <japi/org_openvpp_vppjapi_vppApi.h>

#include <api/vpe_msg_enum.h>
#define vl_typedefs             /* define message structures */
#include <api/vpe_all_api_h.h> 
#undef vl_typedefs

#define vl_endianfun 
#include <api/vpe_all_api_h.h> 
#undef vl_endianfun

/* instantiate all the print functions we know about */
#define vl_print(handle, ...)
#define vl_printfun
#include <api/vpe_all_api_h.h>
#undef vl_printfun

static int connect_to_vpe(char *name);

/* 
 * The Java runtime isn't compile w/ -fstack-protector,
 * so we have to supply missing external references for the
 * regular vpp libraries. Weak reference in case folks get religion
 * at a later date...
 */
void __stack_chk_guard (void) __attribute__((weak));
void __stack_chk_guard (void) {  }

void vl_client_add_api_signatures (vl_api_memclnt_create_t *mp) 
{
    /* 
     * Send the main API signature in slot 0. This bit of code must
     * match the checks in ../vpe/api/api.c: vl_msg_api_version_check().
     */
    mp->api_versions[0] = clib_host_to_net_u32 (vpe_api_version);
}

/* Note: non-static, called once to set up the initial intfc table */
static int sw_interface_dump (vppjni_main_t * jm)
{
    vl_api_sw_interface_dump_t *mp;
    f64 timeout;
    hash_pair_t * p;
    name_sort_t * nses = 0, * ns;
    sw_interface_subif_t * sub = NULL;

    /* Toss the old name table */
    hash_foreach_pair (p, jm->sw_if_index_by_interface_name, 
    ({
        vec_add2 (nses, ns, 1);
        ns->name = (u8 *)(p->key);
        ns->value = (u32) p->value[0];
    }));

    hash_free (jm->sw_if_index_by_interface_name);

    vec_foreach (ns, nses)
        vec_free (ns->name);

    vec_free (nses);

    vec_foreach (sub, jm->sw_if_subif_table) {
        vec_free (sub->interface_name);
    }
    vec_free (jm->sw_if_subif_table);

    /* recreate the interface name hash table */
    jm->sw_if_index_by_interface_name 
      = hash_create_string (0, sizeof(uword));

    /* Get list of ethernets */
    M(SW_INTERFACE_DUMP, sw_interface_dump);
    mp->name_filter_valid = 1;
    strncpy ((char *) mp->name_filter, "Ether", sizeof(mp->name_filter-1)); 
    S;

    /* and local / loopback interfaces */
    M(SW_INTERFACE_DUMP, sw_interface_dump);
    mp->name_filter_valid = 1;
    strncpy ((char *) mp->name_filter, "lo", sizeof(mp->name_filter-1)); 
    S;

    /* and vxlan tunnel interfaces */
    M(SW_INTERFACE_DUMP, sw_interface_dump);
    mp->name_filter_valid = 1;
    strncpy ((char *) mp->name_filter, "vxlan", sizeof(mp->name_filter-1)); 
    S;

    /* and tap tunnel interfaces */
    M(SW_INTERFACE_DUMP, sw_interface_dump);
    mp->name_filter_valid = 1;
    strncpy ((char *) mp->name_filter, "tap", sizeof(mp->name_filter-1)); 
    S;

    /* and l2tpv3 tunnel interfaces */
    M(SW_INTERFACE_DUMP, sw_interface_dump);
    mp->name_filter_valid = 1;
    strncpy ((char *) mp->name_filter, "l2tpv3_tunnel", 
             sizeof(mp->name_filter-1));
    S;

    /* Use a control ping for synchronization */
    {
        vl_api_control_ping_t * mp;
        M(CONTROL_PING, control_ping);
        S;
    }
    W;
}

JNIEXPORT jobject JNICALL Java_org_openvpp_vppjapi_vppConn_getVppVersion
  (JNIEnv *env, jobject obj)
{
  vppjni_main_t * jm = &vppjni_main;
  jmethodID constr;
  // TODO: cache this
  jclass cls = (*env)->FindClass(env, "org/openvpp/vppjapi/vppVersion");
  if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      return NULL;
  }

  constr = (*env)->GetMethodID(env, cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      return NULL;
  }

  vppjni_lock (jm, 11);
  jstring progName = (*env)->NewStringUTF(env, (char *)jm->program_name);
  jstring buildDir = (*env)->NewStringUTF(env, (char *)jm->build_directory);
  jstring gitBranch = (*env)->NewStringUTF(env, (char *)jm->git_branch);
  jstring buildDate = (*env)->NewStringUTF(env, (char *)jm->build_date);
  vppjni_unlock (jm);

  return (*env)->NewObject(env, cls, constr, progName, buildDir, gitBranch, buildDate);
}

static int jm_show_version (vppjni_main_t *jm)
{
  int rv;
  vl_api_show_version_t *mp;
  f64 timeout;

  vppjni_lock (jm, 10);
  M(SHOW_VERSION, show_version);

  S;
  vppjni_unlock (jm);
  WNR;
  return rv;
}

static int jm_stats_enable_disable (vppjni_main_t *jm, u8 enable)
{
  vl_api_want_stats_t * mp;
  f64 timeout;
  int rv;

  vppjni_lock (jm, 13);

  M(WANT_STATS, want_stats);

  mp->enable_disable = enable;

  S;
  vppjni_unlock (jm);
  WNR;

  // already subscribed / already disabled (it's ok)
  if (rv == -2 || rv == -3)
      rv = 0;
  return rv;
}

JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_clientConnect
  (JNIEnv *env, jobject obj, jstring clientName)
{
  int rv;
  const char *client_name;
  void vl_msg_reply_handler_hookup(void);
  vppjni_main_t * jm = &vppjni_main;
  api_main_t * am = &api_main;
  u8 * heap;
  mheap_t * h; 
  f64 timeout;
      
  /* 
   * Bail out now if we're not running as root
   */
  if (geteuid() != 0)
    return -1;

  if (jm->is_connected)
      return -2;

  if (jm->heap == 0)
    clib_mem_init (0, 128<<20);

  heap = clib_mem_get_per_cpu_heap();
  h = mheap_header (heap);

  client_name = (*env)->GetStringUTFChars (env, clientName, 0);

  clib_time_init (&jm->clib_time);

  rv = connect_to_vpe ((char *) client_name);

  if (rv < 0)
    clib_warning ("connection failed, rv %d", rv);

  (*env)->ReleaseStringUTFChars (env, clientName, client_name);

  if (rv == 0)
    {
      vl_msg_reply_handler_hookup ();
      jm->is_connected = 1;
      /* make the main heap thread-safe */
      h->flags |= MHEAP_FLAG_THREAD_SAFE;

      jm->reply_hash = hash_create (0, sizeof (uword));
      jm->callback_hash = hash_create (0, sizeof (uword));
      jm->ping_hash = hash_create (0, sizeof (uword));
      jm->api_main = am;
      vjbd_main_init(&jm->vjbd_main);
      jm->sw_if_index_by_interface_name = 
        hash_create_string (0, sizeof (uword));

      {
        // call control ping first to attach rx thread to java thread
        vl_api_control_ping_t * mp;
        M(CONTROL_PING, control_ping);
        S;
        WNR;

        if (rv != 0) {
            clib_warning ("first control ping failed: %d", rv);
        }
      }
      rv = jm_show_version(jm);
      if (rv != 0)
          clib_warning ("unable to retrieve vpp version (rv: %d)", rv);
      rv = sw_interface_dump(jm);
      if (rv != 0)
          clib_warning ("unable to retrieve interface list (rv: %d)", rv);
      rv = jm_stats_enable_disable(jm, 1);
      if (rv != 0)
          clib_warning ("unable to subscribe to stats (rv: %d)", rv);
    }
  clib_warning ("clientConnect result: %d", rv);

  return rv;
}

JNIEXPORT void JNICALL Java_org_openvpp_vppjapi_vppConn_clientDisconnect
  (JNIEnv *env, jobject obj)
{
  u8 *save_heap;
  vppjni_main_t * jm = &vppjni_main;
  vl_client_disconnect_from_vlib();
  
  save_heap = jm->heap;
  memset (jm, 0, sizeof (*jm));
  jm->heap = save_heap;
}

void vl_api_generic_reply_handler (vl_api_generic_reply_t *mp)
{
  api_main_t * am = &api_main;
  u16 msg_id = clib_net_to_host_u16 (mp->_vl_msg_id);
  trace_cfg_t *cfgp;
  i32 retval = clib_net_to_host_u32 (mp->retval);
  int total_bytes = sizeof(mp);
  vppjni_main_t * jm = &vppjni_main;
  u8 * saved_reply = 0;
  u32 context = clib_host_to_net_u32 (mp->context);
    
  cfgp = am->api_trace_cfg + msg_id;

  if (!cfgp) 
    clib_warning ("msg id %d: no trace configuration\n", msg_id);
  else
    total_bytes = cfgp->size;

  jm->context_id_received = context;

  clib_warning("Received generic reply for msg id %d", msg_id);

  /* A generic reply, successful, we're done */
  if (retval >= 0 && total_bytes == sizeof(*mp))
    return;

  /* Save the reply */
  vec_validate (saved_reply, total_bytes - 1);
  memcpy (saved_reply, mp, total_bytes);

  vppjni_lock (jm, 2);
  hash_set (jm->reply_hash, context, saved_reply);
  jm->saved_reply_count ++;
  vppjni_unlock (jm);
}

JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_getRetval
(JNIEnv * env, jobject obj, jint context, jint release)
{
  vppjni_main_t * jm = &vppjni_main;
  vl_api_generic_reply_t * mp;
  uword * p;
  int rv = 0;

  /* Dunno yet? */
  if (context > jm->context_id_received)
    return (VNET_API_ERROR_RESPONSE_NOT_READY);

  vppjni_lock (jm, 1);
  p = hash_get (jm->reply_hash, context);

  /* 
   * Two cases: a generic "yes" reply - won't be in the hash table
   * or "no", or "more data" which will be in the table.
   */
  if (p == 0)
    goto out;
      
  mp = (vl_api_generic_reply_t *) (p[0]);
  rv = clib_net_to_host_u32 (mp->retval);

  if (release)
    {
      u8 * free_me = (u8 *) mp;
      vec_free (free_me);
      hash_unset (jm->reply_hash, context);
      jm->saved_reply_count --;
    }

out:
  vppjni_unlock (jm);
  return (rv);
}

JNIEXPORT jstring JNICALL Java_org_openvpp_vppjapi_vppConn_getInterfaceList
  (JNIEnv * env, jobject obj, jstring name_filter)
{
  vppjni_main_t * jm = &vppjni_main;
  jstring rv;
  hash_pair_t * p;
  name_sort_t * nses = 0, * ns;
  const char *this_name;
  const char * nf = (*env)->GetStringUTFChars (env, name_filter, NULL);
  u8 * s = 0;
  char *strcasestr (const char *, const char *);

  vppjni_lock (jm, 4);
  
  hash_foreach_pair (p, jm->sw_if_index_by_interface_name, 
    ({
      this_name = (const char *)(p->key);
      if (strlen (nf) == 0 || strcasestr (this_name, nf))
        {
          vec_add2 (nses, ns, 1);
          ns->name = (u8 *)(p->key);
          ns->value = (u32) p->value[0];
        }
    }));

  vec_sort (nses, n1, n2, 
            strcmp ((char *)n1->name, (char *)n2->name));

  vec_foreach (ns, nses)
    s = format (s, "%s: %d, ", ns->name, ns->value);

  _vec_len (s) = vec_len (s) - 2;
  vec_terminate_c_string (s);
  vppjni_unlock (jm);

  vec_free (nses);

  (*env)->ReleaseStringUTFChars (env, name_filter, nf);

  rv = (*env)->NewStringUTF (env, (char *) s);
  vec_free (s);
  
  return rv;
}

JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_swIfIndexFromName
  (JNIEnv * env, jobject obj, jstring interfaceName)
{
  vppjni_main_t * jm = &vppjni_main;
  jint rv = -1;
  const char * if_name = (*env)->GetStringUTFChars (env, interfaceName, NULL);
  uword * p;

  vppjni_lock (jm, 5);

  p = hash_get_mem (jm->sw_if_index_by_interface_name, if_name);

  if (p != 0)
      rv = (jint) p[0];

  vppjni_unlock (jm);
  
  (*env)->ReleaseStringUTFChars (env, interfaceName, if_name);

  return rv;
}

JNIEXPORT jobject JNICALL Java_org_openvpp_vppjapi_vppConn_getInterfaceCounters
(JNIEnv * env, jobject obj, jint swIfIndex)
{
    vppjni_main_t * jm = &vppjni_main;
    sw_interface_stats_t *s;
    u32 sw_if_index = swIfIndex;
    jmethodID constr;
    jobject result = NULL;

    // TODO: cache this
    jclass cls = (*env)->FindClass(env, "org/openvpp/vppjapi/vppInterfaceCounters");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        return NULL;
    }

    constr = (*env)->GetMethodID(env, cls, "<init>", "(JJJJJJJJJJJJJJJJJJJJJJ)V");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        return NULL;
    }

    vppjni_lock (jm, 16);

    if (sw_if_index >= vec_len(jm->sw_if_stats_by_sw_if_index)) {
        goto out;
    }
    s = &jm->sw_if_stats_by_sw_if_index[sw_if_index];
    if (!s->valid) {
        goto out;
    }

    result = (*env)->NewObject(env, cls, constr,
            s->rx.octets, s->rx.pkts.ip4, s->rx.pkts.ip6, s->rx.pkts.unicast,
            s->rx.pkts.multicast, s->rx.pkts.broadcast, s->rx.pkts.discard,
            s->rx.pkts.fifo_full, s->rx.pkts.error, s->rx.pkts.unknown_proto,
            s->rx.pkts.miss,
            s->tx.octets, s->tx.pkts.ip4, s->tx.pkts.ip6, s->tx.pkts.unicast,
            s->tx.pkts.multicast, s->tx.pkts.broadcast, s->tx.pkts.discard,
            s->tx.pkts.fifo_full, s->tx.pkts.error, s->tx.pkts.unknown_proto,
            s->tx.pkts.miss);

out:
    vppjni_unlock (jm);
    return result;
}

JNIEXPORT jstring JNICALL Java_org_openvpp_vppjapi_vppConn_interfaceNameFromSwIfIndex
(JNIEnv * env, jobject obj, jint swIfIndex)
{
    vppjni_main_t * jm = &vppjni_main;
    sw_interface_details_t *sw_if_details;
    u32 sw_if_index;
    jstring ifname = NULL;

    vppjni_lock (jm, 8);

    sw_if_index = swIfIndex;

    if (sw_if_index >= vec_len(jm->sw_if_table)) {
        goto out;
    }
    sw_if_details = &jm->sw_if_table[sw_if_index];
    if (!sw_if_details->valid) {
        goto out;
    }

    u8 * s = format (0, "%s%c", sw_if_details->interface_name, 0);
    ifname = (*env)->NewStringUTF(env, (char *)s);

out:
    vppjni_unlock (jm);

    return ifname;
}

JNIEXPORT void JNICALL Java_org_openvpp_vppjapi_vppConn_clearInterfaceTable
(JNIEnv * env, jobject obj)
{
    vppjni_main_t * jm = &vppjni_main;

    vppjni_lock (jm, 10);

    vec_reset_length(jm->sw_if_table);

    vppjni_unlock (jm);
}


JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_swInterfaceDump
(JNIEnv * env, jobject obj, jbyte name_filter_valid, jbyteArray name_filter)
{
    vppjni_main_t *jm = &vppjni_main;
    f64 timeout;
    vl_api_sw_interface_dump_t * mp;
    u32 my_context_id;
    int rv;
    rv = vppjni_sanity_check (jm);
    if (rv) return rv;
    vppjni_lock (jm, 7);
    my_context_id = vppjni_get_context_id (jm);
    jbyte * name_filterP = (*env)->GetByteArrayElements (env, name_filter, NULL);
    int cnt = (*env)->GetArrayLength (env, name_filter);

    M(SW_INTERFACE_DUMP, sw_interface_dump);
    mp->context = clib_host_to_net_u32 (my_context_id);
    mp->name_filter_valid = name_filter_valid;

    if (cnt > sizeof(mp->name_filter))
        cnt = sizeof(mp->name_filter);

    memcpy ((char *) mp->name_filter, name_filterP, cnt);
    (*env)->ReleaseByteArrayElements (env, name_filter, name_filterP, 0);

    clib_warning("interface filter (%d, %s, len: %d)", mp->name_filter_valid, (char *)mp->name_filter, cnt);

    hash_set (jm->callback_hash, my_context_id, (*env)->NewGlobalRef(env, obj));
    jm->collect_indices = 1;

    S;
    {
        // now send control ping so we know when it ends
        vl_api_control_ping_t * mp;
        M(CONTROL_PING, control_ping);
        mp->context = clib_host_to_net_u32 (my_context_id);

        // this control ping will mark end of interface dump
        hash_set (jm->ping_hash, my_context_id, VL_API_SW_INTERFACE_DUMP);

        S;
    }
    vppjni_unlock (jm);
    WNR;
    return my_context_id;
}

static int sw_if_dump_call_all_callbacks (jobject obj)
{
    vppjni_main_t * jm = &vppjni_main;
    sw_interface_details_t *sw_if_details;
    int rc = 0;
    u32 i;

    JNIEnv *env = jm->jenv;

    for (i = 0; i < vec_len(jm->sw_if_dump_if_indices); i++) {
        u32 sw_if_index = jm->sw_if_dump_if_indices[i];
        ASSERT(sw_if_index < vec_len(jm->sw_if_table));
        sw_if_details = &jm->sw_if_table[sw_if_index];
        ASSERT(sw_if_details->valid);

        u8 * s = format (0, "%s%c", sw_if_details->interface_name, 0);

        jstring ifname = (*env)->NewStringUTF(env, (char *)s);
        jint ifIndex = sw_if_details->sw_if_index;
        jint supIfIndex = sw_if_details->sup_sw_if_index;
        jbyteArray physAddr = (*env)->NewByteArray(env,
                sw_if_details->l2_address_length);
        (*env)->SetByteArrayRegion(env, physAddr, 0,
                sw_if_details->l2_address_length,
                (signed char*)sw_if_details->l2_address);
        jint subId = sw_if_details->sub_id;
        jint subOuterVlanId = sw_if_details->sub_outer_vlan_id;
        jint subInnerVlanId = sw_if_details->sub_inner_vlan_id;
        jint vtrOp = sw_if_details->vtr_op;
        jint vtrPushDot1q = sw_if_details->vtr_push_dot1q;
        jint vtrTag1 = sw_if_details->vtr_tag1;
        jint vtrTag2 = sw_if_details->vtr_tag2;

        jmethodID jmtdIfDetails = jm->jmtdIfDetails;

        jbyte adminUpDown = sw_if_details->admin_up_down;
        jbyte linkUpDown = sw_if_details->link_up_down;
        jbyte linkDuplex = sw_if_details->link_duplex;
        jbyte linkSpeed = sw_if_details->link_speed;
        jbyte subDot1ad = sw_if_details->sub_dot1ad;
        jbyte subNumberOfTags = sw_if_details->sub_number_of_tags;
        jbyte subExactMatch = sw_if_details->sub_exact_match;
        jbyte subDefault = sw_if_details->sub_default;
        jbyte subOuterVlanIdAny = sw_if_details->sub_outer_vlan_id_any;
        jbyte subInnerVlanIdAny = sw_if_details->sub_inner_vlan_id_any;

        clib_warning("Method: %p, Calling method", jm->jmtdIfDetails);
        (*env)->CallVoidMethod(env, obj, jmtdIfDetails, ifIndex, ifname,
                supIfIndex, physAddr, adminUpDown, linkUpDown,
                linkDuplex, linkSpeed, subId, subDot1ad,
                subNumberOfTags, subOuterVlanId, subInnerVlanId,
                subExactMatch, subDefault, subOuterVlanIdAny,
                subInnerVlanIdAny, vtrOp, vtrPushDot1q, vtrTag1, vtrTag2);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            rc = 1;
            goto out;
        }
    }
    clib_warning("Method: %p, Calling method (null arg)", jm->jmtdIfDetails);
    jmethodID jmtdIfDetails = jm->jmtdIfDetails;
    (*env)->CallVoidMethod(env, obj, jmtdIfDetails, -1, NULL, -1, NULL, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rc = 1;
        goto out;
    }

    clib_warning("Done");

out:
    jm->collect_indices = 0;
    vec_reset_length(jm->sw_if_dump_if_indices);
    return rc;
}

JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_findOrAddBridgeDomainId
  (JNIEnv * env, jobject obj, jstring bridgeDomain)
{
  vppjni_main_t * jm = &vppjni_main;
  jint rv = -1;
  const char * bd_name = (*env)->GetStringUTFChars (env, bridgeDomain, NULL);

  vppjni_lock (jm, 6);

  rv = (jint)vjbd_find_or_add_bd (&jm->vjbd_main, (u8 *)bd_name);

  vppjni_unlock (jm);
  
  (*env)->ReleaseStringUTFChars (env, bridgeDomain, bd_name);

  return rv;
}

JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_bridgeDomainIdFromName
  (JNIEnv * env, jobject obj, jstring bridgeDomain)
{
  vppjni_main_t * jm = &vppjni_main;
  jint rv = -1;
  const char * bd_name = (*env)->GetStringUTFChars(env, bridgeDomain, NULL);

  vppjni_lock(jm, 7);

  rv = (jint)vjbd_id_from_name(&jm->vjbd_main, (u8 *)bd_name);

  vppjni_unlock(jm);
  
  (*env)->ReleaseStringUTFChars(env, bridgeDomain, bd_name);

  return rv;
}

JNIEXPORT jint JNICALL Java_org_openvpp_vppjapi_vppConn_bridgeDomainIdFromInterfaceName
  (JNIEnv * env, jobject obj, jstring interfaceName)
{
  vppjni_main_t * jm = &vppjni_main;
  vjbd_main_t * bdm = &jm->vjbd_main;
  u32 sw_if_index;
  jint rv = -1;
  const char * if_name;
  uword * p;

  if_name = (*env)->GetStringUTFChars (env, interfaceName, NULL);

  vppjni_lock (jm, 14);

  p = hash_get_mem (jm->sw_if_index_by_interface_name, if_name);

  if (p != 0) {
      sw_if_index = (jint) p[0];
      p = hash_get (bdm->bd_id_by_sw_if_index, sw_if_index);
      if (p != 0) {
          rv = (jint) p[0];
      }
  }

  vppjni_unlock (jm);

  (*env)->ReleaseStringUTFChars (env, interfaceName, if_name);

  return rv;
}

/* 
 * Special-case: build the interface table, maintain
 * the next loopback sw_if_index vbl.
 */
static void vl_api_sw_interface_details_t_handler
(vl_api_sw_interface_details_t * mp)
{
  vppjni_main_t * jm = &vppjni_main;
  static sw_interface_details_t empty_sw_if_details = {0,};
  sw_interface_details_t *sw_if_details;
  u32 sw_if_index;

  vppjni_lock (jm, 1);

  sw_if_index = ntohl (mp->sw_if_index);

  u8 * s = format (0, "%s%c", mp->interface_name, 0);

  if (jm->collect_indices) {
      u32 pos = vec_len(jm->sw_if_dump_if_indices);
      vec_validate(jm->sw_if_dump_if_indices, pos);
      jm->sw_if_dump_if_indices[pos] = sw_if_index;
  }

  vec_validate_init_empty(jm->sw_if_table, sw_if_index, empty_sw_if_details);
  sw_if_details = &jm->sw_if_table[sw_if_index];
  sw_if_details->valid = 1;

  snprintf((char *)sw_if_details->interface_name,
          sizeof(sw_if_details->interface_name), "%s", (char *)s);
  sw_if_details->sw_if_index = sw_if_index;
  sw_if_details->sup_sw_if_index = ntohl(mp->sup_sw_if_index);
  sw_if_details->l2_address_length = ntohl (mp->l2_address_length);
  ASSERT(sw_if_details->l2_address_length <= sizeof(sw_if_details->l2_address));
  memcpy(sw_if_details->l2_address, mp->l2_address,
          sw_if_details->l2_address_length);
  sw_if_details->sub_id = ntohl (mp->sub_id);
  sw_if_details->sub_outer_vlan_id = ntohl (mp->sub_outer_vlan_id);
  sw_if_details->sub_inner_vlan_id = ntohl (mp->sub_inner_vlan_id);
  sw_if_details->vtr_op = ntohl (mp->vtr_op);
  sw_if_details->vtr_push_dot1q = ntohl (mp->vtr_push_dot1q);
  sw_if_details->vtr_tag1 = ntohl (mp->vtr_tag1);
  sw_if_details->vtr_tag2 = ntohl (mp->vtr_tag2);

  sw_if_details->admin_up_down = mp->admin_up_down;
  sw_if_details->link_up_down = mp->link_up_down;
  sw_if_details->link_duplex = mp->link_duplex;
  sw_if_details->link_speed = mp->link_speed;
  sw_if_details->sub_dot1ad = mp->sub_dot1ad;
  sw_if_details->sub_number_of_tags = mp->sub_number_of_tags;
  sw_if_details->sub_exact_match = mp->sub_exact_match;
  sw_if_details->sub_default = mp->sub_default;
  sw_if_details->sub_outer_vlan_id_any = mp->sub_outer_vlan_id_any;
  sw_if_details->sub_inner_vlan_id_any = mp->sub_inner_vlan_id_any;

  hash_set_mem (jm->sw_if_index_by_interface_name, s, sw_if_index);
  clib_warning("Got interface %s", (char *)s);

  /* In sub interface case, fill the sub interface table entry */
  if (mp->sw_if_index != mp->sup_sw_if_index) {
    sw_interface_subif_t * sub = NULL;

    vec_add2(jm->sw_if_subif_table, sub, 1);

    vec_validate(sub->interface_name, strlen((char *)s) + 1);
    strncpy((char *)sub->interface_name, (char *)s,
            vec_len(sub->interface_name));
    sub->sw_if_index = ntohl(mp->sw_if_index);
    sub->sub_id = ntohl(mp->sub_id);

    sub->sub_dot1ad = mp->sub_dot1ad;
    sub->sub_number_of_tags = mp->sub_number_of_tags;
    sub->sub_outer_vlan_id = ntohs(mp->sub_outer_vlan_id);
    sub->sub_inner_vlan_id = ntohs(mp->sub_inner_vlan_id);
    sub->sub_exact_match = mp->sub_exact_match;
    sub->sub_default = mp->sub_default;
    sub->sub_outer_vlan_id_any = mp->sub_outer_vlan_id_any;
    sub->sub_inner_vlan_id_any = mp->sub_inner_vlan_id_any;

    /* vlan tag rewrite */
    sub->vtr_op = ntohl(mp->vtr_op);
    sub->vtr_push_dot1q = ntohl(mp->vtr_push_dot1q);
    sub->vtr_tag1 = ntohl(mp->vtr_tag1);
    sub->vtr_tag2 = ntohl(mp->vtr_tag2);
  }
  vppjni_unlock (jm);
}

static void vl_api_sw_interface_set_flags_t_handler
(vl_api_sw_interface_set_flags_t * mp)
{
  /* $$$ nothing for the moment */
}

static jintArray create_array_of_bd_ids(JNIEnv * env, jint bd_id)
{
    vppjni_main_t *jm = &vppjni_main;
    vjbd_main_t * bdm = &jm->vjbd_main;
    u32 *buf = NULL;
    u32 i;

    if (bd_id != ~0) {
        vec_add1(buf, bd_id);
    } else {
        for (i = 0; i < vec_len(bdm->bd_oper); i++) {
            u32 bd_id = bdm->bd_oper[i].bd_id;
            vec_add1(buf, bd_id);
        }
    }

    jintArray bdidArray = (*env)->NewIntArray(env, vec_len(buf));

    (*env)->SetIntArrayRegion(env, bdidArray, 0, vec_len(buf), (int*)buf);

    return bdidArray;
}

static void bridge_domain_oper_free(void)
{
    vppjni_main_t *jm = &vppjni_main;
    vjbd_main_t *bdm = &jm->vjbd_main;
    u32 i;

    for (i = 0; i < vec_len(bdm->bd_oper); i++) {
        vec_free(bdm->bd_oper->l2fib_oper);
    }
    vec_reset_length(bdm->bd_oper);
    hash_free(bdm->bd_id_by_sw_if_index);
    hash_free(bdm->oper_bd_index_by_bd_id);
}

JNIEXPORT jintArray JNICALL Java_org_openvpp_vppjapi_vppConn_bridgeDomainDump
(JNIEnv * env, jobject obj, jint bd_id)
{
    vppjni_main_t *jm = &vppjni_main;
    vl_api_bridge_domain_dump_t * mp;
    u32 my_context_id;
    f64 timeout;
    int rv;
    rv = vppjni_sanity_check (jm);
    if (rv) return NULL;

    vppjni_lock (jm, 15);

    if (~0 == bd_id) {
        bridge_domain_oper_free();
    }

    my_context_id = vppjni_get_context_id (jm);
    M(BRIDGE_DOMAIN_DUMP, bridge_domain_dump);
    mp->context = clib_host_to_net_u32 (my_context_id);
    mp->bd_id = clib_host_to_net_u32(bd_id);
    S;

    /* Use a control ping for synchronization */
    {
        vl_api_control_ping_t * mp;
        M(CONTROL_PING, control_ping);
        S;
    }

    WNR;
    if (0 != rv) {
        return NULL;
    }

    jintArray ret = create_array_of_bd_ids(env, bd_id);

    vppjni_unlock (jm);

    return ret;
}

static void
vl_api_bridge_domain_details_t_handler (vl_api_bridge_domain_details_t * mp)
{
  vppjni_main_t *jm = &vppjni_main;
  vjbd_main_t * bdm = &jm->vjbd_main;
  vjbd_oper_t * bd_oper;
  u32 bd_id, bd_index;

  bd_id = ntohl (mp->bd_id);

  bd_index = vec_len(bdm->bd_oper);
  vec_validate (bdm->bd_oper, bd_index);
  bd_oper = vec_elt_at_index(bdm->bd_oper, bd_index);

  hash_set(bdm->oper_bd_index_by_bd_id, bd_id, bd_index);

  bd_oper->bd_id = bd_id;
  bd_oper->flood = mp->flood != 0;
  bd_oper->forward = mp->forward != 0;
  bd_oper->learn =  mp->learn != 0;
  bd_oper->uu_flood = mp->flood != 0;
  bd_oper->arp_term = mp->arp_term != 0;
  bd_oper->bvi_sw_if_index = ntohl (mp->bvi_sw_if_index);
  bd_oper->n_sw_ifs = ntohl (mp->n_sw_ifs);

  bd_oper->bd_sw_if_oper = 0;
}

static void
vl_api_bridge_domain_sw_if_details_t_handler
(vl_api_bridge_domain_sw_if_details_t * mp)
{
  vppjni_main_t *jm = &vppjni_main;
  vjbd_main_t * bdm = &jm->vjbd_main;
  bd_sw_if_oper_t * bd_sw_if_oper;
  u32 bd_id, sw_if_index;

  bd_id = ntohl (mp->bd_id);
  sw_if_index = ntohl (mp->sw_if_index);

  uword *p;
  p = hash_get (bdm->oper_bd_index_by_bd_id, bd_id);
  if (p == 0) {
      clib_warning("Invalid bd_id %d in bridge_domain_sw_if_details_t_handler", bd_id);
      return;
  }
  u32 oper_bd_index = (jint) p[0];
  vjbd_oper_t *bd_oper = vec_elt_at_index(bdm->bd_oper, oper_bd_index);

  u32 len = vec_len(bd_oper->bd_sw_if_oper);
  vec_validate(bd_oper->bd_sw_if_oper, len);
  bd_sw_if_oper = &bd_oper->bd_sw_if_oper[len];
  bd_sw_if_oper->bd_id = bd_id;
  bd_sw_if_oper->sw_if_index = sw_if_index;
  bd_sw_if_oper->shg = mp->shg;

  hash_set(bdm->bd_id_by_sw_if_index, sw_if_index, bd_id);
}

static const char* interface_name_from_sw_if_index(u32 sw_if_index)
{
    vppjni_main_t *jm = &vppjni_main;

    if (sw_if_index >= vec_len(jm->sw_if_table)) {
        return NULL;
    }
    if (!jm->sw_if_table[sw_if_index].valid) {
        return NULL;
    }
    return (const char*)jm->sw_if_table[sw_if_index].interface_name;
}

JNIEXPORT jobject JNICALL Java_org_openvpp_vppjapi_vppConn_getBridgeDomainDetails
(JNIEnv * env, jobject obj, jint bdId)
{
    vppjni_main_t *jm = &vppjni_main;
    vjbd_main_t * bdm = &jm->vjbd_main;
    u32 oper_bd_index;
    u32 bd_id = bdId;
    jobject rv = NULL;
    uword *p;

    vppjni_lock (jm, 16);

    p = hash_get (bdm->oper_bd_index_by_bd_id, bd_id);
    if (p == 0) {
        rv = NULL;
        goto out;
    }
    oper_bd_index = (jint) p[0];

    vjbd_oper_t *bd_oper = vec_elt_at_index(bdm->bd_oper, oper_bd_index);


    /* setting BridgeDomainDetails */

    jclass bddClass = (*env)->FindClass(env, "org/openvpp/vppjapi/vppBridgeDomainDetails");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }

    jmethodID midInit = (*env)->GetMethodID(env, bddClass, "<init>", "()V");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    jobject bddObj = (*env)->NewObject(env, bddClass, midInit);

    u8 *vec_bd_name = vjbd_oper_name_from_id(bdm, bd_id);
    if (NULL == vec_bd_name) {
        rv = NULL;
        goto out;
    }
    char *str_bd_name = (char*)format (0, "%s%c", vec_bd_name, 0);
    vec_free(vec_bd_name);
    jstring bdName = (*env)->NewStringUTF(env, str_bd_name);
    vec_free(str_bd_name);
    if (NULL == bdName) {
        rv = NULL;
        goto out;
    }
    jfieldID fidName = (*env)->GetFieldID(env, bddClass, "name", "Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetObjectField(env, bddObj, fidName, bdName);

    jfieldID fidBdId = (*env)->GetFieldID(env, bddClass, "bdId", "I");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetIntField(env, bddObj, fidBdId, bdId);

    jboolean flood = bd_oper->flood;
    jfieldID fidFlood = (*env)->GetFieldID(env, bddClass, "flood", "Z");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetBooleanField(env, bddObj, fidFlood, flood);

    jboolean uuFlood = bd_oper->uu_flood;
    jfieldID fidUuFlood = (*env)->GetFieldID(env, bddClass, "uuFlood", "Z");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetBooleanField(env, bddObj, fidUuFlood, uuFlood);

    jboolean forward = bd_oper->forward;
    jfieldID fidForward = (*env)->GetFieldID(env, bddClass, "forward", "Z");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetBooleanField(env, bddObj, fidForward, forward);

    jboolean learn = bd_oper->learn;
    jfieldID fidLearn = (*env)->GetFieldID(env, bddClass, "learn", "Z");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetBooleanField(env, bddObj, fidLearn, learn);

    jboolean arpTerm = bd_oper->arp_term;
    jfieldID fidArpTerm = (*env)->GetFieldID(env, bddClass, "arpTerm", "Z");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetBooleanField(env, bddObj, fidArpTerm, arpTerm);

    jstring bviInterfaceName = NULL;
    if (~0 != bd_oper->bvi_sw_if_index) {
        const char *str_if_name = interface_name_from_sw_if_index(bd_oper->bvi_sw_if_index);
        if (NULL == str_if_name) {
            clib_warning("Could not get interface name for sw_if_index %d", bd_oper->bvi_sw_if_index);
            rv = NULL;
            goto out;
        }
        bviInterfaceName = (*env)->NewStringUTF(env, str_if_name);
        if (NULL == bviInterfaceName) {
            rv = NULL;
            goto out;
        }
    }
    jfieldID fidBviInterfaceName = (*env)->GetFieldID(env, bddClass, "bviInterfaceName", "Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetObjectField(env, bddObj, fidBviInterfaceName, bviInterfaceName);


    /* setting BridgeDomainInterfaceDetails */

    jclass bdidClass = (*env)->FindClass(env, "org/openvpp/vppjapi/vppBridgeDomainInterfaceDetails");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }

    jfieldID fidInterfaceName = (*env)->GetFieldID(env, bdidClass, "interfaceName", "Ljava/lang/String;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }

    jfieldID fidSHG = (*env)->GetFieldID(env, bdidClass, "splitHorizonGroup", "B");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }

    u32 len = vec_len(bd_oper->bd_sw_if_oper);
    ASSERT(len == bd_oper->n_sw_ifs);

    jobjectArray bdidArray = (*env)->NewObjectArray(env, len, bdidClass, NULL);

    u32 i;
    for (i = 0; i < len; i++) {
        bd_sw_if_oper_t *sw_if_oper = &bd_oper->bd_sw_if_oper[i];

        jmethodID midBdidInit = (*env)->GetMethodID(env, bdidClass, "<init>", "()V");
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            rv = NULL;
            goto out;
        }
        jobject bdidObj = (*env)->NewObject(env, bdidClass, midBdidInit);
        (*env)->SetObjectArrayElement(env, bdidArray, i, bdidObj);

        u32 sw_if_index = sw_if_oper->sw_if_index;
        const char *str_if_name = interface_name_from_sw_if_index(sw_if_index);
        if (NULL == str_if_name) {
            rv = NULL;
            goto out;
        }
        jstring interfaceName = (*env)->NewStringUTF(env, str_if_name);
        if (NULL == interfaceName) {
            rv = NULL;
            goto out;
        }
        (*env)->SetObjectField(env, bdidObj, fidInterfaceName, interfaceName);

        jbyte shg = sw_if_oper->shg;
        (*env)->SetByteField(env, bdidObj, fidSHG, shg);
    }

    jfieldID fidInterfaces = (*env)->GetFieldID(env, bddClass, "interfaces",
            "[Lorg/openvpp/vppjapi/vppBridgeDomainInterfaceDetails;");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        rv = NULL;
        goto out;
    }
    (*env)->SetObjectField(env, bddObj, fidInterfaces, bdidArray);

    rv = bddObj;

out:

    vppjni_unlock (jm);

    return rv;
}

static jobject l2_fib_create_object(JNIEnv *env, bd_l2fib_oper_t *l2_fib)
{
    jclass l2FibClass = (*env)->FindClass(env, "org/openvpp/vppjapi/vppL2Fib");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        return NULL;
    }

    jmethodID midL2FIbInit = (*env)->GetMethodID(env, l2FibClass, "<init>", "([BZLjava/lang/String;ZZ)V");
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        return NULL;
    }

    u32 sw_if_index = l2_fib->sw_if_index;
    const char *str_if_name = interface_name_from_sw_if_index(sw_if_index);
    if (NULL == str_if_name) {
        return NULL;
    }
    jstring interfaceName = (*env)->NewStringUTF(env, str_if_name);
    if (NULL == interfaceName) {
        return NULL;
    }

    jbyteArray physAddr = (*env)->NewByteArray(env, 6);
    (*env)->SetByteArrayRegion(env, physAddr, 0, 6,
            (signed char*)l2_fib->mac_addr.fields.mac);
    jboolean staticConfig = !l2_fib->learned;
    jstring outgoingInterface = interfaceName;
    jboolean filter = l2_fib->filter;
    jboolean bridgedVirtualInterface = l2_fib->bvi;

    jobject l2FibObj = (*env)->NewObject(env, l2FibClass, midL2FIbInit,
            physAddr, staticConfig, outgoingInterface, filter,
            bridgedVirtualInterface);

    return l2FibObj;
}

JNIEXPORT jobjectArray JNICALL Java_org_openvpp_vppjapi_vppConn_l2FibTableDump
(JNIEnv * env, jobject obj, jint bd_id)
{
  vppjni_main_t *jm = &vppjni_main;
  vjbd_main_t * bdm = &jm->vjbd_main;
  vl_api_l2_fib_table_dump_t *mp;
  jobjectArray l2FibArray = NULL;
  vjbd_oper_t *bd_oper;
  u32 oper_bd_index;
  uword *p;
  f64 timeout;
  int rv;
  u32 i;

  vppjni_lock (jm, 17);

  //vjbd_l2fib_oper_reset (bdm);

  p = hash_get (bdm->oper_bd_index_by_bd_id, bd_id);
  if (p == 0) {
      goto done;
  }
  oper_bd_index = p[0];
  bd_oper = vec_elt_at_index(bdm->bd_oper, oper_bd_index);
  vec_reset_length (bd_oper->l2fib_oper);

  /* Get list of l2 fib table entries */
  M(L2_FIB_TABLE_DUMP, l2_fib_table_dump);
  mp->bd_id = ntohl(bd_id);
  S;

  /* Use a control ping for synchronization */
  {
    vl_api_control_ping_t * mp;
    M(CONTROL_PING, control_ping);
    S;
  }

  WNR;
  if (0 != rv) {
      goto done;
  }

  u32 count = vec_len(bd_oper->l2fib_oper);
  bd_l2fib_oper_t *l2fib_oper = bd_oper->l2fib_oper;

  jclass l2FibClass = (*env)->FindClass(env, "org/openvpp/vppjapi/vppL2Fib");
  if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      goto done;
  }

  l2FibArray = (*env)->NewObjectArray(env, count, l2FibClass, NULL);

  u32 pos = 0;
  for (i = 0; i < count; i++) {
      bd_l2fib_oper_t *l2_fib = &l2fib_oper[i];
      if (1) {
          jobject l2FibObj = l2_fib_create_object(env, l2_fib);
          (*env)->SetObjectArrayElement(env, l2FibArray, pos, l2FibObj);
          pos++;
      }
  }

done:
  vppjni_unlock (jm);

  return l2FibArray;
}

static void
vl_api_l2_fib_table_entry_t_handler (vl_api_l2_fib_table_entry_t * mp)
{
  //static u8 * mac_addr;
  vppjni_main_t *jm = &vppjni_main;
  vjbd_main_t * bdm = &jm->vjbd_main;
  vjbd_oper_t * bd_oper;
  u32 bd_id, oper_bd_index;
  //uword mhash_val_l2fi;
  bd_l2fib_oper_t * l2fib_oper;
  l2fib_u64_mac_t * l2fe_u64_mac = (l2fib_u64_mac_t *)&mp->mac;

  bd_id = ntohl (mp->bd_id);

  uword *p = hash_get (bdm->oper_bd_index_by_bd_id, bd_id);
  if (p == 0) {
      return;
  }
  oper_bd_index = (jint) p[0];
  bd_oper = vec_elt_at_index(bdm->bd_oper, oper_bd_index);

#if 0
  vec_validate (mac_addr, MAC_ADDRESS_SIZE);
  memcpy (mac_addr, l2fe_u64_mac->fields.mac, MAC_ADDRESS_SIZE);
  mhash_val_l2fi = vec_len (bd_oper->l2fib_oper);
  if (mhash_elts (&bd_oper->l2fib_index_by_mac) == 0)
    mhash_init (&bd_oper->l2fib_index_by_mac, sizeof (u32), MAC_ADDRESS_SIZE);
  mhash_set_mem (&bd_oper->l2fib_index_by_mac, mac_addr, &mhash_val_l2fi, 0);
#endif

  vec_add2 (bd_oper->l2fib_oper, l2fib_oper, 1);

  l2fib_oper->bd_id = bd_id;
  l2fib_oper->mac_addr.raw = l2fib_mac_to_u64 (l2fe_u64_mac->fields.mac);
  l2fib_oper->sw_if_index = ntohl (mp->sw_if_index);
  l2fib_oper->learned = !mp->static_mac;
  l2fib_oper->filter = mp->filter_mac;
  l2fib_oper->bvi = mp->bvi_mac;
}

/* cleanup handler for RX thread */
void cleanup_rx_thread(void *arg)
{
  vppjni_main_t * jm = &vppjni_main;

  vppjni_lock (jm, 99);

  int getEnvStat = (*jm->jvm)->GetEnv(jm->jvm, (void **)&(jm->jenv), JNI_VERSION_1_6);
  if (getEnvStat == JNI_EVERSION) {
    clib_warning ("Unsupported JNI version\n");
    jm->retval = -999;
    goto out;
  } else if (getEnvStat != JNI_EDETACHED) {
    (*jm->jvm)->DetachCurrentThread(jm->jvm);
  }
out:
  vppjni_unlock (jm);
}

static void
vl_api_show_version_reply_t_handler (vl_api_show_version_reply_t * mp)
{
  vppjni_main_t * jm = &vppjni_main;
  i32 retval = ntohl(mp->retval);

  if (retval >= 0) {
    clib_warning ("show version request succeeded(%d)");
    strncpy((char*)jm->program_name, (const char*)mp->program,
            sizeof(jm->program_name)-1);
    jm->program_name[sizeof(jm->program_name)-1] = 0;

    strncpy((char*)jm->build_directory, (const char*)mp->build_directory,
            sizeof(jm->build_directory)-1);
    jm->build_directory[sizeof(jm->build_directory)-1] = 0;

    strncpy((char*)jm->git_branch, (const char*)mp->git_branch,
            sizeof(jm->git_branch)-1);
    jm->git_branch[sizeof(jm->git_branch)-1] = 0;

    strncpy((char*)jm->build_date, (const char*)mp->build_date,
            sizeof(jm->build_date)-1);
    jm->build_date[sizeof(jm->build_date)-1] = 0;
  } else {
    clib_error ("show version request failed(%d)", retval);
  }
  jm->retval = retval;
  jm->result_ready = 1;
}

static void vl_api_want_stats_reply_t_handler (vl_api_want_stats_reply_t * mp)
{
  vppjni_main_t * jm = &vppjni_main;
  jm->retval = mp->retval; // FIXME: vpp api does not do ntohl on this retval
  jm->result_ready = 1;
}

// control ping needs to be very first thing called
// to attach rx thread to java thread
static void vl_api_control_ping_reply_t_handler
(vl_api_control_ping_reply_t * mp)
{
  vppjni_main_t * jm = &vppjni_main;
  i32 retval = ntohl(mp->retval);
  u32 context = clib_host_to_net_u32 (mp->context);
  uword *p = NULL;

  jm->retval = retval;

  // attach to java thread if not attached
  int getEnvStat = (*jm->jvm)->GetEnv(jm->jvm, (void **)&(jm->jenv), JNI_VERSION_1_6);
  if (getEnvStat == JNI_EDETACHED) {
    if ((*jm->jvm)->AttachCurrentThread(jm->jvm, (void **)&(jm->jenv), NULL) != 0) {
      clib_warning("Failed to attach thread\n");
      jm->retval = -999;
      goto out;
    }

    // workaround as we can't use pthread_cleanup_push
    pthread_key_create(&jm->cleanup_rx_thread_key, cleanup_rx_thread);
    // destructor is only called if the value of key is non null
    pthread_setspecific(jm->cleanup_rx_thread_key, (void *)1);
  } else if (getEnvStat == JNI_EVERSION) {
    clib_warning ("Unsupported JNI version\n");
    jm->retval = -999;
    goto out;
  }
  // jm->jenv is now stable global reference that can be reused

  // get issuer msg-id
  p = hash_get (jm->ping_hash, context);
  if (p != 0) { // ping marks end of some dump call
    JNIEnv *env = jm->jenv;
    u16 msg_id = (u16)p[0];

    // we will no longer need this
    hash_unset (jm->ping_hash, context);

    // get original caller obj
    p = hash_get (jm->callback_hash, context);

    if (p == 0) // don't have callback stored
      goto out;

    jobject obj = (jobject)p[0]; // object that called original call

    switch (msg_id) {
      case VL_API_SW_INTERFACE_DUMP:
        if (0 != sw_if_dump_call_all_callbacks(obj)) {
          goto out2;
        }
        break;
      default:
        clib_warning("Unhandled control ping issuer msg-id: %d", msg_id);
        goto out2;
        break;
    }
  out2:
    // free the saved obj
    hash_unset (jm->callback_hash, context);
    // delete global reference
    (*env)->DeleteGlobalRef(env, obj);
  }
out:
  jm->result_ready = 1;
}

#define VPPJNI_DEBUG_COUNTERS 0

static void vl_api_vnet_interface_counters_t_handler
(vl_api_vnet_interface_counters_t *mp)
{
  vppjni_main_t *jm = &vppjni_main;
  CLIB_UNUSED(char *counter_name);
  u32 count, sw_if_index;
  int i;
  static sw_interface_stats_t empty_stats = {0, };

  vppjni_lock (jm, 12);
  count = ntohl (mp->count);
  sw_if_index = ntohl (mp->first_sw_if_index);
  if (mp->is_combined == 0) {
    u64 * vp, v;
    vp = (u64 *) mp->data;

    for (i = 0; i < count; i++) {
      sw_interface_details_t *sw_if = NULL;

      v = clib_mem_unaligned (vp, u64);
      v = clib_net_to_host_u64 (v);
      vp++;

      if (sw_if_index < vec_len(jm->sw_if_table))
        sw_if = vec_elt_at_index(jm->sw_if_table, sw_if_index);

      if (sw_if /* && (sw_if->admin_up_down == 1)*/ && sw_if->interface_name[0] != 0)
        {
          vec_validate_init_empty(jm->sw_if_stats_by_sw_if_index, sw_if_index, empty_stats);
          sw_interface_stats_t * s = vec_elt_at_index(jm->sw_if_stats_by_sw_if_index, sw_if_index);

          s->sw_if_index = sw_if_index;
          s->valid = 1;

          switch (mp->vnet_counter_type) {
          case  VNET_INTERFACE_COUNTER_DROP:
            counter_name = "drop";
            s->rx.pkts.discard = v;
            break;
          case  VNET_INTERFACE_COUNTER_PUNT:
            counter_name = "punt";
            s->rx.pkts.unknown_proto = v;
            break;
          case  VNET_INTERFACE_COUNTER_IP4:
            counter_name = "ip4";
            s->rx.pkts.ip4 = v;
            break;
          case  VNET_INTERFACE_COUNTER_IP6:
            counter_name = "ip6";
            s->rx.pkts.ip6 = v;
            break;
          case  VNET_INTERFACE_COUNTER_RX_NO_BUF:
            counter_name = "rx-no-buf";
            s->rx.pkts.fifo_full = v;
            break;
          case  VNET_INTERFACE_COUNTER_RX_MISS:
            counter_name = "rx-miss";
            s->rx.pkts.miss = v;
            break;
          case  VNET_INTERFACE_COUNTER_RX_ERROR:
            counter_name = "rx-error";
            s->rx.pkts.error = v;
            break;
          case  VNET_INTERFACE_COUNTER_TX_ERROR:
            counter_name = "tx-error (fifo-full)";
            s->tx.pkts.fifo_full = v;
            break;
          default:
            counter_name = "bogus";
            break;
          }

#if VPPJNI_DEBUG_COUNTERS == 1
          clib_warning ("%s (%d): %s (%lld)\n", sw_if->interface_name, s->sw_if_index,
                        counter_name, v);
#endif
        }
      sw_if_index++;
    }
  } else {
    vlib_counter_t *vp;
    u64 packets, bytes;
    vp = (vlib_counter_t *) mp->data;

    for (i = 0; i < count; i++) {
      sw_interface_details_t *sw_if = NULL;

      packets = clib_mem_unaligned (&vp->packets, u64);
      packets = clib_net_to_host_u64 (packets);
      bytes = clib_mem_unaligned (&vp->bytes, u64);
      bytes = clib_net_to_host_u64 (bytes);
      vp++;

      if (sw_if_index < vec_len(jm->sw_if_table))
        sw_if = vec_elt_at_index(jm->sw_if_table, sw_if_index);

      if (sw_if /* && (sw_if->admin_up_down == 1) */ && sw_if->interface_name[0] != 0)
        {
          vec_validate_init_empty(jm->sw_if_stats_by_sw_if_index, sw_if_index, empty_stats);
          sw_interface_stats_t * s = vec_elt_at_index(jm->sw_if_stats_by_sw_if_index, sw_if_index);

          s->valid = 1;
          s->sw_if_index = sw_if_index;

          switch (mp->vnet_counter_type) {
          case  VNET_INTERFACE_COUNTER_RX:
            s->rx.pkts.unicast = packets;
            s->rx.octets = bytes;
            counter_name = "rx";
            break;

          case  VNET_INTERFACE_COUNTER_TX:
            s->tx.pkts.unicast = packets;
            s->tx.octets = bytes;
            counter_name = "tx";
            break;

          default:
            counter_name = "bogus";
            break;
          }

#if VPPJNI_DEBUG_COUNTERS == 1
          clib_warning ("%s (%d): %s.packets %lld\n", 
                   sw_if->interface_name,
                   sw_if_index, counter_name, packets);
          clib_warning ("%s (%d): %s.bytes %lld\n", 
                   sw_if->interface_name,
                   sw_if_index, counter_name, bytes);
#endif
        }
      sw_if_index++;
    }
  }
  vppjni_unlock (jm);
}

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  vppjni_main_t * jm = &vppjni_main;
  JNIEnv* env;
  if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  jclass cls = (*env)->FindClass(env, "org/openvpp/vppjapi/vppApiCallbacks");
  if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      return JNI_ERR;
  }

  jm->jmtdIfDetails = 
      (*env)->GetMethodID(env, cls, "interfaceDetails", "(ILjava/lang/String;I[BBBBBIBBIIBBBBIIII)V");
  if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionDescribe(env);
      return JNI_ERR;
  }

  // methods are not local references they stay forever
  // jclass is local reference let's hold global ref to it
  jm->jcls = (*env)->NewGlobalRef(env, cls);

  jm->jvm = vm;
  return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM *vm, void *reserved) {
  vppjni_main_t * jm = &vppjni_main;
  JNIEnv* env;
  if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    return;
  }

  if (jm->jcls != NULL) {
    (*env)->DeleteGlobalRef(env, jm->jcls);
    jm->jcls = NULL;
  }

  jm->jenv = NULL;
  jm->jvm = NULL;
}

#define foreach_vpe_api_msg                             \
_(CONTROL_PING_REPLY, control_ping_reply)               \
_(SW_INTERFACE_DETAILS, sw_interface_details)           \
_(SHOW_VERSION_REPLY, show_version_reply)               \
_(WANT_STATS_REPLY, want_stats_reply)                   \
_(VNET_INTERFACE_COUNTERS, vnet_interface_counters)     \
_(SW_INTERFACE_SET_FLAGS, sw_interface_set_flags)       \
_(BRIDGE_DOMAIN_DETAILS, bridge_domain_details)         \
_(BRIDGE_DOMAIN_SW_IF_DETAILS, bridge_domain_sw_if_details) \
_(L2_FIB_TABLE_ENTRY, l2_fib_table_entry)

static int connect_to_vpe(char *name)
{
  vppjni_main_t * jm = &vppjni_main;
  api_main_t * am = &api_main;

  if (vl_client_connect_to_vlib("/vpe-api", name, 32) < 0)
    return -1;

  jm->my_client_index = am->my_client_index;
  jm->vl_input_queue = am->shmem_hdr->vl_input_queue;

#define _(N,n)                                                  \
    vl_msg_api_set_handlers(VL_API_##N, #n,                     \
                           vl_api_##n##_t_handler,	        \
                           vl_noop_handler,                     \
                           vl_api_##n##_t_endian,               \
                           vl_api_##n##_t_print,                \
                           sizeof(vl_api_##n##_t), 1); 
    foreach_vpe_api_msg;
#undef _
  

  return 0;
}

/* Format an IP6 address. */
u8 * format_ip6_address (u8 * s, va_list * args)
{
  ip6_address_t * a = va_arg (*args, ip6_address_t *);
  u32 max_zero_run = 0, this_zero_run = 0;
  int max_zero_run_index = -1, this_zero_run_index=0;
  int in_zero_run = 0, i;
  int last_double_colon = 0;

  /* Ugh, this is a pain. Scan forward looking for runs of 0's */
  for (i = 0; i < ARRAY_LEN (a->as_u16); i++)
    {
      if (a->as_u16[i] == 0)
        {
          if (in_zero_run)
            this_zero_run++;
          else
            {
              in_zero_run = 1;
              this_zero_run =1;
              this_zero_run_index = i;
            }
        }
      else
        {
          if (in_zero_run)
            {
              /* offer to compress the biggest run of > 1 zero */
              if (this_zero_run > max_zero_run && this_zero_run > 1)
                {
                  max_zero_run_index = this_zero_run_index;
                  max_zero_run = this_zero_run;
                }
            }
          in_zero_run = 0;
          this_zero_run = 0;
        }
    }

  if (in_zero_run)
    {
      if (this_zero_run > max_zero_run && this_zero_run > 1)
        {
          max_zero_run_index = this_zero_run_index;
          max_zero_run = this_zero_run;
        }
    }
  
  for (i = 0; i < ARRAY_LEN (a->as_u16); i++)
    {
      if (i == max_zero_run_index)
        {
	  s = format (s, "::");
          i += max_zero_run - 1;
          last_double_colon = 1;
        }
      else
	{
	  s = format (s, "%s%x",
		      (last_double_colon || i == 0) ? "" : ":",
		      clib_net_to_host_u16 (a->as_u16[i]));
	  last_double_colon = 0;
	}
    }

  return s;
}

/* Format an IP4 address. */
u8 * format_ip4_address (u8 * s, va_list * args)
{
  u8 * a = va_arg (*args, u8 *);
  return format (s, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
}


