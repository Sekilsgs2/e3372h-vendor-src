#ifndef _NFCT_EXTERN_H_
#define _NFCT_EXTERN_H_

extern const set_attr 	set_attr_array[];
extern const get_attr 	get_attr_array[];
extern const copy_attr 	copy_attr_array[];
extern const filter_attr 	filter_attr_array[];
extern const set_attr_grp	set_attr_grp_array[];
extern const get_attr_grp	get_attr_grp_array[];

extern const set_exp_attr	set_exp_attr_array[];
extern const get_exp_attr	get_exp_attr_array[];

extern const uint32_t attr_grp_bitmask[ATTR_GRP_MAX][__NFCT_BITSET];

/* for the snprintf infrastructure */
extern const char *const l3proto2str[AF_MAX];
extern const char *const proto2str[IPPROTO_MAX];
extern const char *const states[TCP_CONNTRACK_MAX];
extern const char *const sctp_states[SCTP_CONNTRACK_MAX];
extern const char *const dccp_states[DCCP_CONNTRACK_MAX];

#endif
