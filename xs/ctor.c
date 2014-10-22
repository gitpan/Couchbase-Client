#include "perl-couchbase.h"

void plcb_ctor_cbc_opts(AV *options, struct lcb_create_st *cropts)
{
    
#define _assign_options(dst, opt_idx, defl) \
    if ( (tmp = av_fetch(options, opt_idx, 0)) && SvTRUE(*tmp) ) { \
        dst = SvPV_nolen(*tmp); \
    } else { \
        dst = defl; \
    }

    SV **tmp;
    
    _assign_options(cropts->v.v0.host, PLCB_CTORIDX_SERVERS, "127.0.0.1:8091");
    _assign_options(cropts->v.v0.user, PLCB_CTORIDX_USERNAME, NULL);
    _assign_options(cropts->v.v0.passwd, PLCB_CTORIDX_PASSWORD, NULL);
    _assign_options(cropts->v.v0.bucket, PLCB_CTORIDX_BUCKET, "default");

#undef _assign_options
}

static void ctor_extract_methpairs(AV *options,
                                   int idx,
                                   SV **outmeth,
                                   SV **inmeth)
{
    SV **tmpsv;
    AV *methav = NULL;
    int ii;
    
    SV **assgn_array[] = { outmeth, inmeth };
    
    *outmeth = *inmeth = NULL;
    if ( (tmpsv = av_fetch(options, idx, 0)) == NULL ) {
        return;
    }
    
    if (SvROK(*tmpsv) == 0 ||
        ((methav = (AV*)SvRV(*tmpsv)) && SvTYPE(methav) != SVt_PVAV) ||
        av_len(methav) != 1) {
        die("Expected an array reference with two elements");
    }
    
    for (ii = 0; ii < 2; ii++) {
        tmpsv = av_fetch(methav, ii, 0);

        if (SvROK(*tmpsv) == 0 || SvTYPE(SvRV(*tmpsv)) != SVt_PVCV) {
            die("Expected code reference.");
        }

        *(assgn_array[ii]) = newRV_inc(SvRV(*tmpsv));
    }
}

void plcb_ctor_conversion_opts(PLCB_t *object, AV *options)
{
    SV **tmpsv;
    
#define meth_assert_getpairs(flag, optidx) \
    ((object->my_flags & flag) \
    ? \
        (((tmpsv = av_fetch(options, optidx, 0)) && SvROK(*tmpsv) && \
            (methav = (AV*)SvRV(*tmpsv))) \
            ? (void*)1 \
            :  die("Flag %s specified but no methods provided", #flag)) \
    : NULL)
    
#define meth_assert_assign(target_field, source_idx, diemsg) \
    if ((tmpsv = av_fetch(methav, source_idx, 0)) == NULL) { \
        die("Nothing in IDX=%d (%s)", source_idx, diemsg); \
    } \
    if (! ((SvROK(*tmpsv) && SvTYPE(SvRV(*tmpsv)) == SVt_PVCV) ) ) { \
        die("Expected CODE reference at IDX=%d: %s",source_idx, diemsg); \
    } \
    object->target_field = newRV_inc(SvRV(*tmpsv));
    
#define meth_maybe_assign(idx, target, name) \
    if ( (tmpsv = av_fetch(options, idx, 0)) != NULL && SvTYPE(*tmpsv) != SVt_NULL) { \
        if (!SvROK(*tmpsv) || SvTYPE(SvRV(*tmpsv)) != SVt_PVCV) { \
            die("Expected a code reference for %s but found something else", name); \
        } \
        (void)SvREFCNT_inc(*tmpsv); \
        object->target = *tmpsv; \
    }

    if ( (tmpsv = av_fetch(options, PLCB_CTORIDX_MYFLAGS, 0))
       && SvIOK(*tmpsv)) {
        object->my_flags = SvUV(*tmpsv);
    }
    
    ctor_extract_methpairs(options, PLCB_CTORIDX_COMP_METHODS,
                           &object->cv_compress, &object->cv_decompress);
    
    if ((object->my_flags & PLCBf_USE_COMPRESSION) &&
        object->cv_compress == NULL) {
        
        die("Compression requested but no methods provided");
    }
    
    
    ctor_extract_methpairs(options, PLCB_CTORIDX_SERIALIZE_METHODS,
                           &object->cv_serialize, &object->cv_deserialize);
    
    if ((object->my_flags & PLCBf_USE_STORABLE) &&
        object->cv_serialize == NULL) {
        
        die("Serialization requested but no methods provided");
    }
        
    if ((tmpsv = av_fetch(options, PLCB_CTORIDX_COMP_THRESHOLD, 0))
       && SvIOK(*tmpsv)) {
        object->compress_threshold = SvIV(*tmpsv);

    } else {
        object->compress_threshold = 0;
    }

    /* For Couch/JSON */
    meth_maybe_assign(PLCB_CTORIDX_JSON_ENCODE_METHOD,
                      couch.cv_json_encode,
                      "JSON encode");

    meth_maybe_assign(PLCB_CTORIDX_JSON_VERIFY_METHOD,
                      couch.cv_json_verify,
                      "JSON verify");
}

void plcb_ctor_init_common(PLCB_t *object, lcb_t instance, AV *options)
{
    NV timeout_value;
    SV **tmpsv;
    
    object->instance = instance;
    object->errors = newAV();

#define get_stash_assert(stashname, target) \
    if (! (object->target = gv_stashpv(stashname, 0)) ) { \
        die("Couldn't load '%s'", stashname); \
    }
    
    get_stash_assert(PLCB_RET_CLASSNAME, ret_stash);
    get_stash_assert(PLCB_ITER_CLASSNAME, iter_stash);
    get_stash_assert(PLCB_COUCH_HANDLE_INFO_CLASSNAME, couch.handle_av_stash);

#undef get_stash_assert

    /*gather instance-related options from the constructor*/
    if ( (tmpsv = av_fetch(options, PLCB_CTORIDX_TIMEOUT, 0))  &&
            (SvIOK(*tmpsv) || SvNOK(*tmpsv))) {
        timeout_value = SvNV(*tmpsv);

        if (!timeout_value) {
            warn("Cannot use 0 for timeout");

        } else {
            lcb_set_timeout(instance,
                timeout_value * (1000*1000));
        }
    }
    
    if ((tmpsv = av_fetch(options, PLCB_CTORIDX_NO_CONNECT, 0)) &&
       SvTRUE(*tmpsv)) {
        object->my_flags |= PLCBf_NO_CONNECT;
    }
    /*maybe more stuff here?*/
    object->sync.type = PLCB_SYNCTYPE_SINGLE;

    plcb_couch_callbacks_setup(object);
}
