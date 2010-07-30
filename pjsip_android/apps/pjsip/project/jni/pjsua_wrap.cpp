/**
 * This file is part of CSipSimple.
 *
 *  CSipSimple is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  CSipSimple is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CSipSimple.  If not, see <http://www.gnu.org/licenses/>.
 */
#define SWIG_DIRECTORS


//#define SWIG_JAVA_NO_DETACH_CURRENT_THREAD
//#define SWIG_JAVA_ATTACH_CURRENT_THREAD_AS_DAEMON



#include <android/log.h>
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "libpjsip", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , "libpjsip", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , "libpjsip", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , "libpjsip", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , "libpjsip", __VA_ARGS__)


static void on_log_msg(int level, const char *data, int len) {
	if (level <= 1) {
		LOGE("%s", data);
	} else if (level == 2) {
		LOGW("%s", data);
	} else if (level == 3) {
		LOGI("%s", data);
	} else if (level == 4) {
		LOGD("%s", data);
	} else if (level >= 5) {
		LOGV("%s", data);
	}
}


#include <utils/misc.h>

#include <swig_base.h>
#include <pjsua-lib/pjsua.h>
#define THIS_FILE	"pjsua_wrap.cpp"

//---------------------
// RINGBACK MANAGEMENT-
// --------------------
/* Ringtones		    US	       UK  */
#define RINGBACK_FREQ1	    440	    /* 400 */
#define RINGBACK_FREQ2	    480	    /* 450 */
#define RINGBACK_ON	    2000    /* 400 */
#define RINGBACK_OFF	    4000    /* 200 */
#define RINGBACK_CNT	    1	    /* 2   */
#define RINGBACK_INTERVAL   4000    /* 2000 */

static struct app_config {
	pj_pool_t		   *pool;
    int			    ringback_slot;
    int			    ringback_cnt;
    pjmedia_port	   *ringback_port;
    pj_bool_t ringback_on;
} app_config;


static void ringback_start(){
	app_config.ringback_on = PJ_TRUE;

    if (++app_config.ringback_cnt==1 && app_config.ringback_slot!=PJSUA_INVALID_ID)
    {
	pjsua_conf_connect(app_config.ringback_slot, 0);
    }
}

static void ring_stop(pjsua_call_id call_id) {

    if (app_config.ringback_on) {
    	app_config.ringback_on = PJ_FALSE;

		pj_assert(app_config.ringback_cnt>0);
		if (--app_config.ringback_cnt == 0 &&
			app_config.ringback_slot!=PJSUA_INVALID_ID)  {
			pjsua_conf_disconnect(app_config.ringback_slot, 0);
			pjmedia_tonegen_rewind(app_config.ringback_port);
		}
    }
}

static void init_ringback_tone(){
	pj_status_t status;
	pj_str_t name;
	pjmedia_tone_desc tone[RINGBACK_CNT];
	unsigned i;

	app_config.pool = pjsua_pool_create("pjsua-app", 1000, 1000);
	app_config.ringback_slot=PJSUA_INVALID_ID;
	app_config.ringback_on = PJ_FALSE;
	app_config.ringback_cnt = 0;



	//Ringback
	name = pj_str((char *)"ringback");
	status = pjmedia_tonegen_create2(app_config.pool, &name,
					 16000,
					 1,
					 320,
					 16, PJMEDIA_TONEGEN_LOOP,
					 &app_config.ringback_port);
	if (status != PJ_SUCCESS){
		goto on_error;
	}

	pj_bzero(&tone, sizeof(tone));
	for (i=0; i<RINGBACK_CNT; ++i) {
		tone[i].freq1 = RINGBACK_FREQ1;
		tone[i].freq2 = RINGBACK_FREQ2;
		tone[i].on_msec = RINGBACK_ON;
		tone[i].off_msec = RINGBACK_OFF;
	}
	tone[RINGBACK_CNT-1].off_msec = RINGBACK_INTERVAL;

	pjmedia_tonegen_play(app_config.ringback_port, RINGBACK_CNT, tone, PJMEDIA_TONEGEN_LOOP);


	status = pjsua_conf_add_port(app_config.pool, app_config.ringback_port,
					 &app_config.ringback_slot);
	if (status != PJ_SUCCESS){
		goto on_error;
	}
	return;

	on_error :{
		pj_pool_release(app_config.pool);
	}
}

static void destroy_ringback_tone(){
	/* Close ringback port */
	if (app_config.ringback_port &&
	app_config.ringback_slot != PJSUA_INVALID_ID){
		pjsua_conf_remove_port(app_config.ringback_slot);
		app_config.ringback_slot = PJSUA_INVALID_ID;
		pjmedia_port_destroy(app_config.ringback_port);
		app_config.ringback_port = NULL;
	}
}



class Callback {
public:
	virtual ~Callback() {}
	virtual void on_call_state (pjsua_call_id call_id, pjsip_event *e) {}
	virtual void on_incoming_call (pjsua_acc_id acc_id, pjsua_call_id call_id,
		pjsip_rx_data *rdata) {}
	virtual void on_call_tsx_state (pjsua_call_id call_id, 
		pjsip_transaction *tsx,
		pjsip_event *e) {}
	virtual void on_call_media_state (pjsua_call_id call_id) {}
	virtual void on_stream_created (pjsua_call_id call_id, 
		pjmedia_session *sess,
		unsigned stream_idx, 
		pjmedia_port **p_port) {}
	virtual void on_stream_destroyed (pjsua_call_id call_id,
		pjmedia_session *sess, 
		unsigned stream_idx) {}
	virtual void on_dtmf_digit (pjsua_call_id call_id, int digit) {}
	virtual void on_call_transfer_request (pjsua_call_id call_id,
		const pj_str_t *dst,
		pjsip_status_code *code) {}
	virtual void on_call_transfer_status (pjsua_call_id call_id,
		int st_code,
		const pj_str_t *st_text,
		pj_bool_t final_,
		pj_bool_t *p_cont) {}
	virtual void on_call_replace_request (pjsua_call_id call_id,
		pjsip_rx_data *rdata,
		int *st_code,
		pj_str_t *st_text) {}
	virtual void on_call_replaced (pjsua_call_id old_call_id,
		pjsua_call_id new_call_id) {}
	virtual void on_reg_state (pjsua_acc_id acc_id) {}
	virtual void on_buddy_state (pjsua_buddy_id buddy_id) {}
	virtual void on_pager (pjsua_call_id call_id, const pj_str_t *from,
		const pj_str_t *to, const pj_str_t *contact,
		const pj_str_t *mime_type, const pj_str_t *body) {}
	virtual void on_pager2 (pjsua_call_id call_id, const pj_str_t *from,
		const pj_str_t *to, const pj_str_t *contact,
		const pj_str_t *mime_type, const pj_str_t *body,
		pjsip_rx_data *rdata) {}
	virtual void on_pager_status (pjsua_call_id call_id,
		const pj_str_t *to,
		const pj_str_t *body,
/*XXX		void *user_data,*/
		pjsip_status_code status,
		const pj_str_t *reason) {}
	virtual void on_pager_status2 (pjsua_call_id call_id,
		const pj_str_t *to,
		const pj_str_t *body,
/*XXX		void *user_data,*/
		pjsip_status_code status,
		const pj_str_t *reason,
		pjsip_tx_data *tdata,
		pjsip_rx_data *rdata) {}
	virtual void on_typing (pjsua_call_id call_id, const pj_str_t *from,
		const pj_str_t *to, const pj_str_t *contact,
		pj_bool_t is_typing) {}
	virtual void on_nat_detect (const pj_stun_nat_detect_result *res) {}
};

static Callback* registeredCallbackObject = NULL;


extern "C" {
void on_call_state_wrapper(pjsua_call_id call_id, pjsip_event *e) {
	pjsua_call_info call_info;
	pjsua_call_get_info(call_id, &call_info);

	if (call_info.state == PJSIP_INV_STATE_DISCONNECTED) {
		/* Stop all ringback for this call */
		ring_stop(call_id);
		PJ_LOG(3,(THIS_FILE, "Call %d is DISCONNECTED [reason=%d (%s)]",
			  call_id,
			  call_info.last_status,
			  call_info.last_status_text.ptr));
	} else {
		if (call_info.state == PJSIP_INV_STATE_EARLY) {
			int code;
			pj_str_t reason;
			pjsip_msg *msg;

			/* This can only occur because of TX or RX message */
			pj_assert(e->type == PJSIP_EVENT_TSX_STATE);

			if (e->body.tsx_state.type == PJSIP_EVENT_RX_MSG) {
				msg = e->body.tsx_state.src.rdata->msg_info.msg;
			} else {
				msg = e->body.tsx_state.src.tdata->msg;
			}

			code = msg->line.status.code;
			reason = msg->line.status.reason;

			/* Start ringback for 180 for UAC unless there's SDP in 180 */
			if (call_info.role==PJSIP_ROLE_UAC && code==180 &&
			msg->body == NULL &&
			call_info.media_status==PJSUA_CALL_MEDIA_NONE) {
				ringback_start();
			}

			PJ_LOG(3,(THIS_FILE, "Call %d state changed to %s (%d %.*s)",
				  call_id, call_info.state_text.ptr,
				  code, (int)reason.slen, reason.ptr));
		} else {
			PJ_LOG(3,(THIS_FILE, "Call %d state changed to %s",
				  call_id,
				  call_info.state_text.ptr));
		}
	}
	registeredCallbackObject->on_call_state(call_id, e);
}

void on_incoming_call_wrapper (pjsua_acc_id acc_id, pjsua_call_id call_id,
	pjsip_rx_data *rdata) {
	registeredCallbackObject->on_incoming_call(acc_id, call_id, rdata);
}

void on_call_tsx_state_wrapper (pjsua_call_id call_id,
		pjsip_transaction *tsx,
		pjsip_event *e) {
	registeredCallbackObject->on_call_tsx_state(call_id, tsx, e);
}

void on_call_media_state_wrapper (pjsua_call_id call_id) {
	ring_stop(call_id);
	registeredCallbackObject->on_call_media_state(call_id);
}
 

void on_stream_created_wrapper (pjsua_call_id call_id, 
		pjmedia_session *sess,
		unsigned stream_idx, 
		pjmedia_port **p_port) {
	registeredCallbackObject->on_stream_created(call_id, sess, stream_idx, p_port);
}

void on_stream_destroyed_wrapper (pjsua_call_id call_id,
	pjmedia_session *sess, 
	unsigned stream_idx) {
	registeredCallbackObject->on_stream_destroyed(call_id, sess, stream_idx);
}

void on_dtmf_digit_wrapper (pjsua_call_id call_id, int digit) {
	registeredCallbackObject->on_dtmf_digit(call_id, digit);
}

void on_call_transfer_request_wrapper (pjsua_call_id call_id,
	const pj_str_t *dst,
	pjsip_status_code *code) {
	registeredCallbackObject->on_call_transfer_request(call_id, dst, code);
}

void on_call_transfer_status_wrapper (pjsua_call_id call_id,
	int st_code,
	const pj_str_t *st_text,
	pj_bool_t final_,
	pj_bool_t *p_cont) {
	registeredCallbackObject->on_call_transfer_status(call_id, st_code, st_text, final_, p_cont);
}

void on_call_replace_request_wrapper (pjsua_call_id call_id,
	pjsip_rx_data *rdata,
	int *st_code,
	pj_str_t *st_text) {
	registeredCallbackObject->on_call_replace_request(call_id, rdata, st_code, st_text);
}

void on_call_replaced_wrapper (pjsua_call_id old_call_id,
	pjsua_call_id new_call_id) {
	registeredCallbackObject->on_call_replaced(old_call_id, new_call_id);
}

void on_reg_state_wrapper (pjsua_acc_id acc_id) {
	registeredCallbackObject->on_reg_state(acc_id);
}

void on_incoming_subscribe_wrapper (pjsua_acc_id  acc_id, pjsua_srv_pres  *srv_pres, pjsua_buddy_id  buddy_id, const pj_str_t  *from, pjsip_rx_data  *rdata, pjsip_status_code  *code, pj_str_t  *reason, pjsua_msg_data  *msg_data){
	//TODO: implement
}

void on_srv_subscribe_state_wrapper (pjsua_acc_id acc_id, pjsua_srv_pres *srv_pres, const pj_str_t *remote_uri, pjsip_evsub_state state, pjsip_event *event){
	//TODO: implement
}

void on_buddy_state_wrapper (pjsua_buddy_id buddy_id) {
	registeredCallbackObject->on_buddy_state(buddy_id);
}

void on_pager_wrapper (pjsua_call_id call_id, const pj_str_t *from,
	const pj_str_t *to, const pj_str_t *contact,
	const pj_str_t *mime_type, const pj_str_t *body) {
	registeredCallbackObject->on_pager(call_id, from, to, contact, mime_type, body);
}

void on_pager2_wrapper (pjsua_call_id call_id, const pj_str_t *from,
	const pj_str_t *to, const pj_str_t *contact,
	const pj_str_t *mime_type, const pj_str_t *body,
	pjsip_rx_data *rdata, pjsua_acc_id acc_id) {
	registeredCallbackObject->on_pager2(call_id, from, to, contact, mime_type, body, rdata);
}

void on_pager_status_wrapper (pjsua_call_id call_id,
	const pj_str_t *to,
	const pj_str_t *body,
	void *user_data,
	pjsip_status_code status,
	const pj_str_t *reason) {
	registeredCallbackObject->on_pager_status(call_id, to, body, /*XXX user_data,*/ status, reason);
}

void on_pager_status2_wrapper (pjsua_call_id call_id,
	const pj_str_t *to,
	const pj_str_t *body,
	void *user_data,
	pjsip_status_code status,
	const pj_str_t *reason,
	pjsip_tx_data *tdata,
	pjsip_rx_data *rdata, pjsua_acc_id acc_id) {
	registeredCallbackObject->on_pager_status2(call_id, to, body, /*XXX user_data,*/ status, reason, tdata, rdata);
}

void on_typing_wrapper (pjsua_call_id call_id, const pj_str_t *from,
	const pj_str_t *to, const pj_str_t *contact,
	pj_bool_t is_typing) {
	registeredCallbackObject->on_typing(call_id, from, to, contact, is_typing);
}

void on_typing2_wrapper (pjsua_call_id  call_id, const pj_str_t  *from, const pj_str_t  *to, const pj_str_t  *contact, pj_bool_t  is_typing, pjsip_rx_data  *rdata, pjsua_acc_id  acc_id){
	//TODO : implement
}

void on_nat_detect_wrapper (const pj_stun_nat_detect_result *res) {
	registeredCallbackObject->on_nat_detect(res);
}

/*
pjsip_redirect_op on_call_redirected_wrapper (pjsua_call_id  call_id, const pjsip_uri  *target, const pjsip_event  *e){
	LOGI("REDIRECT OP...");

}

void on_mwi_info_wrapper (pjsua_acc_id acc_id, pjsua_mwi_info *mwi_info){

}
*/

}


static struct pjsua_callback wrapper_callback_struct = {
	&on_call_state_wrapper,
	&on_incoming_call_wrapper,
	&on_call_tsx_state_wrapper,
	&on_call_media_state_wrapper,
	&on_stream_created_wrapper,
	&on_stream_destroyed_wrapper,
	&on_dtmf_digit_wrapper,
	&on_call_transfer_request_wrapper,
	&on_call_transfer_status_wrapper,
	&on_call_replace_request_wrapper,
	&on_call_replaced_wrapper,
	&on_reg_state_wrapper,

//	&on_incoming_subscribe_wrapper,
//	&on_srv_subscribe_state_wrapper,
	NULL,
	NULL,

	&on_buddy_state_wrapper,
	&on_pager_wrapper,
	&on_pager2_wrapper,
	&on_pager_status_wrapper,
	&on_pager_status2_wrapper,
	&on_typing_wrapper,
	NULL,
	&on_nat_detect_wrapper,
	NULL
};


void setCallbackObject(Callback* callback) {
	registeredCallbackObject = callback;
}



pj_str_t pj_str_copy(const char *str) {
	size_t length = strlen(str) + 1;
	char* copy = (char*)calloc(length, sizeof(char));
	copy = strncpy(copy, str, length);
	return pj_str(copy);
}

pj_status_t get_snd_dev_info( pjmedia_snd_dev_info* info, int id )
{
    unsigned dev_count;
	const pjmedia_snd_dev_info *ci;

    dev_count = pjmedia_aud_dev_count();

	PJ_ASSERT_RETURN(id >= 0 && id < dev_count, PJ_EINVAL);
	

	//TODO : reimplement
	//ci = pjmedia_snd_get_dev_info(id);
	//pj_memcpy(info, ci, sizeof(*ci));

    return PJ_SUCCESS;
}



typedef struct {
        pj_uint8_t  frm_per_pkt;    /**< Number of frames per packet.   */
        unsigned    vad:1;          /**< Voice Activity Detector.       */
        unsigned    cng:1;          /**< Comfort Noise Generator.       */
        unsigned    penh:1;         /**< Perceptual Enhancement         */
        unsigned    plc:1;          /**< Packet loss concealment        */
        unsigned    reserved:1;     /**< Reserved, must be zero.        */
        pj_uint8_t  enc_fmtp_mode;  /**< Mode param in fmtp (def:0)     */
        pj_uint8_t  dec_fmtp_mode;  /**< Mode param in fmtp (def:0)     */
    } pjmedia_codec_param_setting;



typedef struct {
       unsigned    clock_rate;          /**< Sampling rate in Hz            */
       unsigned    channel_cnt;         /**< Channel count.                 */
       pj_uint32_t avg_bps;             /**< Average bandwidth in bits/sec  */
       pj_uint32_t max_bps;             /**< Maximum bandwidth in bits/sec  */
       pj_uint16_t frm_ptime;           /**< Decoder frame ptime in msec.   */
       pj_uint16_t enc_ptime;           /**< Encoder ptime, or zero if it's
                                             equal to decoder ptime.        */
       pj_uint8_t  pcm_bits_per_sample; /**< Bits/sample in the PCM side    */
       pj_uint8_t  pt;                  /**< Payload type.                  */
    } pjmedia_codec_param_info;



typedef struct {
        void            *pdata;             /**< Pointer data.      */
        long             ldata;             /**< Long data.         */
    } pjmedia_port_port_data;



typedef union {
        /** Digest AKA credential information. Note that when AKA credential
         *  is being used, the \a data field of this #pjsip_cred_info is
         *  not used, but it still must be initialized to an empty string.
         * Please see \ref PJSIP_AUTH_AKA_API for more information.
         */
        struct {
            pj_str_t      k;    /**< Permanent subscriber key.          */
            pj_str_t      op;   /**< Operator variant key.              */
            pj_str_t      amf;  /**< Authentication Management Field    */
            pjsip_cred_cb cb;   /**< Callback to create AKA digest.     */
        } aka;

    } pjsip_cred_info_ext;



typedef struct {
            pj_str_t      k;    /**< Permanent subscriber key.          */
            pj_str_t      op;   /**< Operator variant key.              */
            pj_str_t      amf;  /**< Authentication Management Field    */
            pjsip_cred_cb cb;   /**< Callback to create AKA digest.     */
        } pjsip_cred_info_ext_aka;



typedef union {
        /** Timer event. */
        struct         {
            pj_timer_entry *entry;      /**< The timer entry.           */
        } timer;

        /** Transaction state has changed event. */
        struct         {
            union             {
                pjsip_rx_data   *rdata; /**< The incoming message.      */
                pjsip_tx_data   *tdata; /**< The outgoing message.      */
                pj_timer_entry  *timer; /**< The timer.                 */
                pj_status_t      status;/**< Transport error status.    */
                void            *data;  /**< Generic data.              */
            } src;
            pjsip_transaction   *tsx;   /**< The transaction.           */
            int                  prev_state; /**< Previous state.       */
            pjsip_event_id_e     type;  /**< Type of event source:
                                         *      - PJSIP_EVENT_TX_MSG
                                         *      - PJSIP_EVENT_RX_MSG,
                                         *      - PJSIP_EVENT_TRANSPORT_ERROR
                                         *      - PJSIP_EVENT_TIMER
                                         *      - PJSIP_EVENT_USER
                                         */
        } tsx_state;

        /** Message transmission event. */
        struct         {
            pjsip_tx_data       *tdata; /**< The transmit data buffer.  */

        } tx_msg;

        /** Transmission error event. */
        struct         {
            pjsip_tx_data       *tdata; /**< The transmit data.         */
            pjsip_transaction   *tsx;   /**< The transaction.           */
        } tx_error;

        /** Message arrival event. */
        struct         {
            pjsip_rx_data       *rdata; /**< The receive data buffer.   */
        } rx_msg;

        /** User event. */
        struct         {
            void                *user1; /**< User data 1.               */
            void                *user2; /**< User data 2.               */
            void                *user3; /**< User data 3.               */
            void                *user4; /**< User data 4.               */
        } user;

    } pjsip_event_body;



typedef struct {
            void                *user1; /**< User data 1.               */
            void                *user2; /**< User data 2.               */
            void                *user3; /**< User data 3.               */
            void                *user4; /**< User data 4.               */
        } pjsip_event_body_user;



typedef struct {
            pjsip_rx_data       *rdata; /**< The receive data buffer.   */
        } pjsip_event_body_rx_msg;



typedef struct {
            pjsip_tx_data       *tdata; /**< The transmit data.         */
            pjsip_transaction   *tsx;   /**< The transaction.           */
        } pjsip_event_body_tx_error;



typedef struct {
            pjsip_tx_data       *tdata; /**< The transmit data buffer.  */

        } pjsip_event_body_tx_msg;



typedef struct {
            union             {
                pjsip_rx_data   *rdata; /**< The incoming message.      */
                pjsip_tx_data   *tdata; /**< The outgoing message.      */
                pj_timer_entry  *timer; /**< The timer.                 */
                pj_status_t      status;/**< Transport error status.    */
                void            *data;  /**< Generic data.              */
            } src;
            pjsip_transaction   *tsx;   /**< The transaction.           */
            int                  prev_state; /**< Previous state.       */
            pjsip_event_id_e     type;  /**< Type of event source:
                                         *      - PJSIP_EVENT_TX_MSG
                                         *      - PJSIP_EVENT_RX_MSG,
                                         *      - PJSIP_EVENT_TRANSPORT_ERROR
                                         *      - PJSIP_EVENT_TIMER
                                         *      - PJSIP_EVENT_USER
                                         */
        } pjsip_event_body_tsx_state;



typedef struct {
            pj_timer_entry *entry;      /**< The timer entry.           */
        } pjsip_event_body_timer;



typedef union {
                pjsip_rx_data   *rdata; /**< The incoming message.      */
                pjsip_tx_data   *tdata; /**< The outgoing message.      */
                pj_timer_entry  *timer; /**< The timer.                 */
                pj_status_t      status;/**< Transport error status.    */
                void            *data;  /**< Generic data.              */
            } pjsip_event_body_tsx_state_src;



typedef struct {
	char	local_info[128];
	char	local_contact[128];
	char	remote_info[128];
	char	remote_contact[128];
	char	call_id[128];
	char	last_status_text[128];
    } pjsua_call_info_buf_;




/* ---------------------------------------------------
 * C++ director class methods
 * --------------------------------------------------- */

#include "pjsua_wrap.h"

SwigDirector_Callback::SwigDirector_Callback(JNIEnv *jenv) : Callback(), Swig::Director(jenv) {
}

SwigDirector_Callback::~SwigDirector_Callback() {
  swig_disconnect_director_self("swigDirectorDisconnect");
}


void SwigDirector_Callback::on_call_state(pjsua_call_id call_id, pjsip_event *e) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong je = 0 ;
  
  if (!swig_override[0]) {
    Callback::on_call_state(call_id,e);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pjsip_event **)&je) = (pjsip_event *) e; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[0], swigjobj, jcall_id, je);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jacc_id  ;
  jint jcall_id  ;
  jlong jrdata = 0 ;
  if (!swig_override[1]) {
    Callback::on_incoming_call(acc_id,call_id,rdata);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jacc_id = (jint) acc_id;
    jcall_id = (jint) call_id;
    *((pjsip_rx_data **)&jrdata) = (pjsip_rx_data *) rdata; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[1], swigjobj, jacc_id, jcall_id, jrdata);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_call_tsx_state(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jtsx = 0 ;
  jlong je = 0 ;
  
  if (!swig_override[2]) {
    Callback::on_call_tsx_state(call_id,tsx,e);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pjsip_transaction **)&jtsx) = (pjsip_transaction *) tsx; 
    *((pjsip_event **)&je) = (pjsip_event *) e; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[2], swigjobj, jcall_id, jtsx, je);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_call_media_state(pjsua_call_id call_id) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  
  if (!swig_override[3]) {
    Callback::on_call_media_state(call_id);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[3], swigjobj, jcall_id);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_stream_created(pjsua_call_id call_id, pjmedia_session *sess, unsigned int stream_idx, pjmedia_port **p_port) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jsess = 0 ;
  jlong jstream_idx  ;
  jlong jp_port = 0 ;
  if (!swig_override[4]) {
    Callback::on_stream_created(call_id,sess,stream_idx,p_port);
    return;
  }

  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pjmedia_session **)&jsess) = (pjmedia_session *) sess; 
    jstream_idx = (jlong) stream_idx;
    *((pjmedia_port ***)&jp_port) = (pjmedia_port **) p_port; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[4], swigjobj, jcall_id, jsess, jstream_idx, jp_port);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_stream_destroyed(pjsua_call_id call_id, pjmedia_session *sess, unsigned int stream_idx) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jsess = 0 ;
  jlong jstream_idx  ;
  
  if (!swig_override[5]) {
    Callback::on_stream_destroyed(call_id,sess,stream_idx);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pjmedia_session **)&jsess) = (pjmedia_session *) sess; 
    jstream_idx = (jlong) stream_idx;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[5], swigjobj, jcall_id, jsess, jstream_idx);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_dtmf_digit(pjsua_call_id call_id, int digit) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jint jdigit  ;
  
  if (!swig_override[6]) {
    Callback::on_dtmf_digit(call_id,digit);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    jdigit = (jint) digit;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[6], swigjobj, jcall_id, jdigit);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_call_transfer_request(pjsua_call_id call_id, pj_str_t const *dst, pjsip_status_code *code) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jdst = 0 ;
  jlong jcode = 0 ;
  
  if (!swig_override[7]) {
    Callback::on_call_transfer_request(call_id,dst,code);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pj_str_t **)&jdst) = (pj_str_t *) dst; 
    *((pjsip_status_code **)&jcode) = (pjsip_status_code *) code; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[7], swigjobj, jcall_id, jdst, jcode);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_call_transfer_status(pjsua_call_id call_id, int st_code, pj_str_t const *st_text, pj_bool_t final_, pj_bool_t *p_cont) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jint jst_code  ;
  jlong jst_text = 0 ;
  jint jfinal_  ;
  jlong jp_cont = 0 ;
  
  if (!swig_override[8]) {
    Callback::on_call_transfer_status(call_id,st_code,st_text,final_,p_cont);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    jst_code = (jint) st_code;
    *((pj_str_t **)&jst_text) = (pj_str_t *) st_text; 
    jfinal_ = (jint) final_;
    *((pj_bool_t **)&jp_cont) = (pj_bool_t *) p_cont; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[8], swigjobj, jcall_id, jst_code, jst_text, jfinal_, jp_cont);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_call_replace_request(pjsua_call_id call_id, pjsip_rx_data *rdata, int *st_code, pj_str_t *st_text) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jrdata = 0 ;
  jlong jst_code = 0 ;
  jlong jst_text = 0 ;
  
  if (!swig_override[9]) {
    Callback::on_call_replace_request(call_id,rdata,st_code,st_text);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pjsip_rx_data **)&jrdata) = (pjsip_rx_data *) rdata; 
    *((int **)&jst_code) = (int *) st_code; 
    *((pj_str_t **)&jst_text) = (pj_str_t *) st_text; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[9], swigjobj, jcall_id, jrdata, jst_code, jst_text);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_call_replaced(pjsua_call_id old_call_id, pjsua_call_id new_call_id) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jold_call_id  ;
  jint jnew_call_id  ;
  
  if (!swig_override[10]) {
    Callback::on_call_replaced(old_call_id,new_call_id);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jold_call_id = (jint) old_call_id;
    jnew_call_id = (jint) new_call_id;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[10], swigjobj, jold_call_id, jnew_call_id);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_reg_state(pjsua_acc_id acc_id) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jacc_id  ;
  
  if (!swig_override[11]) {
    Callback::on_reg_state(acc_id);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jacc_id = (jint) acc_id;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[11], swigjobj, jacc_id);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_buddy_state(pjsua_buddy_id buddy_id) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jbuddy_id  ;
  
  if (!swig_override[12]) {
    Callback::on_buddy_state(buddy_id);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jbuddy_id = (jint) buddy_id;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[12], swigjobj, jbuddy_id);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_pager(pjsua_call_id call_id, pj_str_t const *from, pj_str_t const *to, pj_str_t const *contact, pj_str_t const *mime_type, pj_str_t const *body) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jfrom = 0 ;
  jlong jto = 0 ;
  jlong jcontact = 0 ;
  jlong jmime_type = 0 ;
  jlong jbody = 0 ;
  
  if (!swig_override[13]) {
    Callback::on_pager(call_id,from,to,contact,mime_type,body);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pj_str_t **)&jfrom) = (pj_str_t *) from; 
    *((pj_str_t **)&jto) = (pj_str_t *) to; 
    *((pj_str_t **)&jcontact) = (pj_str_t *) contact; 
    *((pj_str_t **)&jmime_type) = (pj_str_t *) mime_type; 
    *((pj_str_t **)&jbody) = (pj_str_t *) body; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[13], swigjobj, jcall_id, jfrom, jto, jcontact, jmime_type, jbody);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_pager2(pjsua_call_id call_id, pj_str_t const *from, pj_str_t const *to, pj_str_t const *contact, pj_str_t const *mime_type, pj_str_t const *body, pjsip_rx_data *rdata) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jfrom = 0 ;
  jlong jto = 0 ;
  jlong jcontact = 0 ;
  jlong jmime_type = 0 ;
  jlong jbody = 0 ;
  jlong jrdata = 0 ;
  
  if (!swig_override[14]) {
    Callback::on_pager2(call_id,from,to,contact,mime_type,body,rdata);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pj_str_t **)&jfrom) = (pj_str_t *) from; 
    *((pj_str_t **)&jto) = (pj_str_t *) to; 
    *((pj_str_t **)&jcontact) = (pj_str_t *) contact; 
    *((pj_str_t **)&jmime_type) = (pj_str_t *) mime_type; 
    *((pj_str_t **)&jbody) = (pj_str_t *) body; 
    *((pjsip_rx_data **)&jrdata) = (pjsip_rx_data *) rdata; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[14], swigjobj, jcall_id, jfrom, jto, jcontact, jmime_type, jbody, jrdata);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_pager_status(pjsua_call_id call_id, pj_str_t const *to, pj_str_t const *body, pjsip_status_code status, pj_str_t const *reason) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jto = 0 ;
  jlong jbody = 0 ;
  jint jstatus  ;
  jlong jreason = 0 ;
  
  if (!swig_override[15]) {
    Callback::on_pager_status(call_id,to,body,status,reason);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pj_str_t **)&jto) = (pj_str_t *) to; 
    *((pj_str_t **)&jbody) = (pj_str_t *) body; 
    jstatus = (jint) status;
    *((pj_str_t **)&jreason) = (pj_str_t *) reason; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[15], swigjobj, jcall_id, jto, jbody, jstatus, jreason);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_pager_status2(pjsua_call_id call_id, pj_str_t const *to, pj_str_t const *body, pjsip_status_code status, pj_str_t const *reason, pjsip_tx_data *tdata, pjsip_rx_data *rdata) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jto = 0 ;
  jlong jbody = 0 ;
  jint jstatus  ;
  jlong jreason = 0 ;
  jlong jtdata = 0 ;
  jlong jrdata = 0 ;
  
  if (!swig_override[16]) {
    Callback::on_pager_status2(call_id,to,body,status,reason,tdata,rdata);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pj_str_t **)&jto) = (pj_str_t *) to; 
    *((pj_str_t **)&jbody) = (pj_str_t *) body; 
    jstatus = (jint) status;
    *((pj_str_t **)&jreason) = (pj_str_t *) reason; 
    *((pjsip_tx_data **)&jtdata) = (pjsip_tx_data *) tdata; 
    *((pjsip_rx_data **)&jrdata) = (pjsip_rx_data *) rdata; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[16], swigjobj, jcall_id, jto, jbody, jstatus, jreason, jtdata, jrdata);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_typing(pjsua_call_id call_id, pj_str_t const *from, pj_str_t const *to, pj_str_t const *contact, pj_bool_t is_typing) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jint jcall_id  ;
  jlong jfrom = 0 ;
  jlong jto = 0 ;
  jlong jcontact = 0 ;
  jint jis_typing  ;
  
  if (!swig_override[17]) {
    Callback::on_typing(call_id,from,to,contact,is_typing);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    jcall_id = (jint) call_id;
    *((pj_str_t **)&jfrom) = (pj_str_t *) from; 
    *((pj_str_t **)&jto) = (pj_str_t *) to; 
    *((pj_str_t **)&jcontact) = (pj_str_t *) contact; 
    jis_typing = (jint) is_typing;
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[17], swigjobj, jcall_id, jfrom, jto, jcontact, jis_typing);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::on_nat_detect(pj_stun_nat_detect_result const *res) {
  JNIEnvWrapper swigjnienv(this) ;
  JNIEnv * jenv = swigjnienv.getJNIEnv() ;
  jobject swigjobj = (jobject) NULL ;
  jlong jres = 0 ;
  
  if (!swig_override[18]) {
    Callback::on_nat_detect(res);
    return;
  }
  swigjobj = swig_get_self(jenv);
  if (swigjobj && jenv->IsSameObject(swigjobj, NULL) == JNI_FALSE) {
    *((pj_stun_nat_detect_result **)&jres) = (pj_stun_nat_detect_result *) res; 
    jenv->CallStaticVoidMethod(Swig::jclass_pjsuaJNI, Swig::director_methids[18], swigjobj, jres);
    if (jenv->ExceptionOccurred()) return ;
  } else {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null upcall object");
  }
  if (swigjobj) jenv->DeleteLocalRef(swigjobj);
}

void SwigDirector_Callback::swig_connect_director(JNIEnv *jenv, jobject jself, jclass jcls, bool swig_mem_own, bool weak_global) {
  static struct {
    const char *mname;
    const char *mdesc;
    jmethodID base_methid;
  } methods[] = {
    {
      "on_call_state", "(ILorg/pjsip/pjsua/pjsip_event;)V", NULL 
    },
    {
      "on_incoming_call", "(IILorg/pjsip/pjsua/SWIGTYPE_p_pjsip_rx_data;)V", NULL 
    },
    {
      "on_call_tsx_state", "(ILorg/pjsip/pjsua/SWIGTYPE_p_pjsip_transaction;Lorg/pjsip/pjsua/pjsip_event;)V", NULL 
    },
    {
      "on_call_media_state", "(I)V", NULL 
    },
    {
      "on_stream_created", "(ILorg/pjsip/pjsua/SWIGTYPE_p_pjmedia_session;JLorg/pjsip/pjsua/SWIGTYPE_p_p_pjmedia_port;)V", NULL 
    },
    {
      "on_stream_destroyed", "(ILorg/pjsip/pjsua/SWIGTYPE_p_pjmedia_session;J)V", NULL 
    },
    {
      "on_dtmf_digit", "(II)V", NULL 
    },
    {
      "on_call_transfer_request", "(ILorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/SWIGTYPE_p_pjsip_status_code;)V", NULL 
    },
    {
      "on_call_transfer_status", "(IILorg/pjsip/pjsua/pj_str_t;ILorg/pjsip/pjsua/SWIGTYPE_p_int;)V", NULL 
    },
    {
      "on_call_replace_request", "(ILorg/pjsip/pjsua/SWIGTYPE_p_pjsip_rx_data;Lorg/pjsip/pjsua/SWIGTYPE_p_int;Lorg/pjsip/pjsua/pj_str_t;)V", NULL 
    },
    {
      "on_call_replaced", "(II)V", NULL 
    },
    {
      "on_reg_state", "(I)V", NULL 
    },
    {
      "on_buddy_state", "(I)V", NULL 
    },
    {
      "on_pager", "(ILorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;)V", NULL 
    },
    {
      "on_pager2", "(ILorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/SWIGTYPE_p_pjsip_rx_data;)V", NULL 
    },
    {
      "on_pager_status", "(ILorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pjsip_status_code;Lorg/pjsip/pjsua/pj_str_t;)V", NULL 
    },
    {
      "on_pager_status2", "(ILorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pjsip_status_code;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/SWIGTYPE_p_pjsip_tx_data;Lorg/pjsip/pjsua/SWIGTYPE_p_pjsip_rx_data;)V", NULL 
    },
    {
      "on_typing", "(ILorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;Lorg/pjsip/pjsua/pj_str_t;I)V", NULL 
    },
    {
      "on_nat_detect", "(Lorg/pjsip/pjsua/SWIGTYPE_p_pj_stun_nat_detect_result;)V", NULL 
    }
  };
  
  static jclass baseclass = 0 ;
  
  if (swig_set_self(jenv, jself, swig_mem_own, weak_global)) {
    if (!baseclass) {
      baseclass = jenv->FindClass("org/pjsip/pjsua/Callback");
      if (!baseclass) return;
      baseclass = (jclass) jenv->NewGlobalRef(baseclass);
    }
    bool derived = (jenv->IsSameObject(baseclass, jcls) ? false : true);
    for (int i = 0; i < 19; ++i) {
      if (!methods[i].base_methid) {
        methods[i].base_methid = jenv->GetMethodID(baseclass, methods[i].mname, methods[i].mdesc);
        if (!methods[i].base_methid) return;
      }
      swig_override[i] = false;
      if (derived) {
        jmethodID methid = jenv->GetMethodID(jcls, methods[i].mname, methods[i].mdesc);
        swig_override[i] = (methid != methods[i].base_methid);
        jenv->ExceptionClear();
      }
    }
  }
}



#ifdef __cplusplus
extern "C" {
#endif

SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1copy(JNIEnv *jenv, jclass jcls, jstring jarg1) {
  jlong jresult = 0 ;
  char *arg1 = (char *) 0 ;
  pj_str_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = 0;
  if (jarg1) {
    arg1 = (char *)jenv->GetStringUTFChars(jarg1, 0);
    if (!arg1) return 0;
  }
  result = pj_str_copy((char const *)arg1);
  *(pj_str_t **)&jresult = new pj_str_t((const pj_str_t &)result); 
  if (arg1) jenv->ReleaseStringUTFChars(jarg1, (const char *)arg1);
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1snd_1dev_1info(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  jint jresult = 0 ;
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  int arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  arg2 = (int)jarg2; 
  result = (pj_status_t)get_snd_dev_info(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1Callback(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  Callback *arg1 = (Callback *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(Callback **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1state(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsip_event *arg3 = (pjsip_event *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjsip_event **)&jarg3; 
  (arg1)->on_call_state(arg2,arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1stateSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsip_event *arg3 = (pjsip_event *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjsip_event **)&jarg3; 
  (arg1)->Callback::on_call_state(arg2,arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1incoming_1call(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3, jlong jarg4) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_acc_id arg2 ;
  pjsua_call_id arg3 ;
  pjsip_rx_data *arg4 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_acc_id)jarg2; 
  arg3 = (pjsua_call_id)jarg3; 
  arg4 = *(pjsip_rx_data **)&jarg4; 
  (arg1)->on_incoming_call(arg2,arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1incoming_1callSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3, jlong jarg4) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_acc_id arg2 ;
  pjsua_call_id arg3 ;
  pjsip_rx_data *arg4 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_acc_id)jarg2; 
  arg3 = (pjsua_call_id)jarg3; 
  arg4 = *(pjsip_rx_data **)&jarg4; 
  (arg1)->Callback::on_incoming_call(arg2,arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1tsx_1state(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4, jobject jarg4_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsip_transaction *arg3 = (pjsip_transaction *) 0 ;
  pjsip_event *arg4 = (pjsip_event *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg4_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjsip_transaction **)&jarg3; 
  arg4 = *(pjsip_event **)&jarg4; 
  (arg1)->on_call_tsx_state(arg2,arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1tsx_1stateSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4, jobject jarg4_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsip_transaction *arg3 = (pjsip_transaction *) 0 ;
  pjsip_event *arg4 = (pjsip_event *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg4_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjsip_transaction **)&jarg3; 
  arg4 = *(pjsip_event **)&jarg4; 
  (arg1)->Callback::on_call_tsx_state(arg2,arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1media_1state(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  (arg1)->on_call_media_state(arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1media_1stateSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  (arg1)->Callback::on_call_media_state(arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1created(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4, jlong jarg5) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjmedia_session *arg3 = (pjmedia_session *) 0 ;
  unsigned int arg4 ;
  pjmedia_port **arg5 = (pjmedia_port **) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjmedia_session **)&jarg3; 
  arg4 = (unsigned int)jarg4; 
  arg5 = *(pjmedia_port ***)&jarg5; 
  (arg1)->on_stream_created(arg2,arg3,arg4,arg5);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1createdSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4, jlong jarg5) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjmedia_session *arg3 = (pjmedia_session *) 0 ;
  unsigned int arg4 ;
  pjmedia_port **arg5 = (pjmedia_port **) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjmedia_session **)&jarg3; 
  arg4 = (unsigned int)jarg4; 
  arg5 = *(pjmedia_port ***)&jarg5; 
  (arg1)->Callback::on_stream_created(arg2,arg3,arg4,arg5);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1destroyed(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjmedia_session *arg3 = (pjmedia_session *) 0 ;
  unsigned int arg4 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjmedia_session **)&jarg3; 
  arg4 = (unsigned int)jarg4; 
  (arg1)->on_stream_destroyed(arg2,arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1destroyedSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjmedia_session *arg3 = (pjmedia_session *) 0 ;
  unsigned int arg4 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjmedia_session **)&jarg3; 
  arg4 = (unsigned int)jarg4; 
  (arg1)->Callback::on_stream_destroyed(arg2,arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1dtmf_1digit(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  int arg3 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (int)jarg3; 
  (arg1)->on_dtmf_digit(arg2,arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1dtmf_1digitSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  int arg3 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (int)jarg3; 
  (arg1)->Callback::on_dtmf_digit(arg2,arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1request(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pjsip_status_code *arg4 = (pjsip_status_code *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pjsip_status_code **)&jarg4; 
  (arg1)->on_call_transfer_request(arg2,(pj_str_t const *)arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1requestSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pjsip_status_code *arg4 = (pjsip_status_code *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pjsip_status_code **)&jarg4; 
  (arg1)->Callback::on_call_transfer_request(arg2,(pj_str_t const *)arg3,arg4);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1status(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3, jlong jarg4, jobject jarg4_, jint jarg5, jlong jarg6) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  int arg3 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_bool_t arg5 ;
  pj_bool_t *arg6 = (pj_bool_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg4_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (int)jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = (pj_bool_t)jarg5; 
  arg6 = *(pj_bool_t **)&jarg6; 
  (arg1)->on_call_transfer_status(arg2,arg3,(pj_str_t const *)arg4,arg5,arg6);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1statusSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3, jlong jarg4, jobject jarg4_, jint jarg5, jlong jarg6) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  int arg3 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_bool_t arg5 ;
  pj_bool_t *arg6 = (pj_bool_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg4_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (int)jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = (pj_bool_t)jarg5; 
  arg6 = *(pj_bool_t **)&jarg6; 
  (arg1)->Callback::on_call_transfer_status(arg2,arg3,(pj_str_t const *)arg4,arg5,arg6);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replace_1request(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4, jlong jarg5, jobject jarg5_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsip_rx_data *arg3 = (pjsip_rx_data *) 0 ;
  int *arg4 = (int *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg5_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjsip_rx_data **)&jarg3; 
  arg4 = *(int **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  (arg1)->on_call_replace_request(arg2,arg3,arg4,arg5);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replace_1requestSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jlong jarg4, jlong jarg5, jobject jarg5_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsip_rx_data *arg3 = (pjsip_rx_data *) 0 ;
  int *arg4 = (int *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg5_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pjsip_rx_data **)&jarg3; 
  arg4 = *(int **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  (arg1)->Callback::on_call_replace_request(arg2,arg3,arg4,arg5);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replaced(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsua_call_id arg3 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (pjsua_call_id)jarg3; 
  (arg1)->on_call_replaced(arg2,arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replacedSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jint jarg3) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pjsua_call_id arg3 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (pjsua_call_id)jarg3; 
  (arg1)->Callback::on_call_replaced(arg2,arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1reg_1state(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_acc_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_acc_id)jarg2; 
  (arg1)->on_reg_state(arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1reg_1stateSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_acc_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_acc_id)jarg2; 
  (arg1)->Callback::on_reg_state(arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1buddy_1state(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_buddy_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_buddy_id)jarg2; 
  (arg1)->on_buddy_state(arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1buddy_1stateSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_buddy_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_buddy_id)jarg2; 
  (arg1)->Callback::on_buddy_state(arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, jlong jarg6, jobject jarg6_, jlong jarg7, jobject jarg7_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  pj_str_t *arg7 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  (void)jarg6_;
  (void)jarg7_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  arg7 = *(pj_str_t **)&jarg7; 
  (arg1)->on_pager(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pj_str_t const *)arg5,(pj_str_t const *)arg6,(pj_str_t const *)arg7);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pagerSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, jlong jarg6, jobject jarg6_, jlong jarg7, jobject jarg7_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  pj_str_t *arg7 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  (void)jarg6_;
  (void)jarg7_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  arg7 = *(pj_str_t **)&jarg7; 
  (arg1)->Callback::on_pager(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pj_str_t const *)arg5,(pj_str_t const *)arg6,(pj_str_t const *)arg7);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager2(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, jlong jarg6, jobject jarg6_, jlong jarg7, jobject jarg7_, jlong jarg8) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  pj_str_t *arg7 = (pj_str_t *) 0 ;
  pjsip_rx_data *arg8 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  (void)jarg6_;
  (void)jarg7_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  arg7 = *(pj_str_t **)&jarg7; 
  arg8 = *(pjsip_rx_data **)&jarg8; 
  (arg1)->on_pager2(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pj_str_t const *)arg5,(pj_str_t const *)arg6,(pj_str_t const *)arg7,arg8);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager2SwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, jlong jarg6, jobject jarg6_, jlong jarg7, jobject jarg7_, jlong jarg8) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  pj_str_t *arg7 = (pj_str_t *) 0 ;
  pjsip_rx_data *arg8 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  (void)jarg6_;
  (void)jarg7_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  arg7 = *(pj_str_t **)&jarg7; 
  arg8 = *(pjsip_rx_data **)&jarg8; 
  (arg1)->Callback::on_pager2(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pj_str_t const *)arg5,(pj_str_t const *)arg6,(pj_str_t const *)arg7,arg8);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1status(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jint jarg5, jlong jarg6, jobject jarg6_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pjsip_status_code arg5 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg6_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = (pjsip_status_code)jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  (arg1)->on_pager_status(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,arg5,(pj_str_t const *)arg6);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1statusSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jint jarg5, jlong jarg6, jobject jarg6_) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pjsip_status_code arg5 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg6_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = (pjsip_status_code)jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  (arg1)->Callback::on_pager_status(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,arg5,(pj_str_t const *)arg6);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1status2(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jint jarg5, jlong jarg6, jobject jarg6_, jlong jarg7, jlong jarg8) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pjsip_status_code arg5 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  pjsip_tx_data *arg7 = (pjsip_tx_data *) 0 ;
  pjsip_rx_data *arg8 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg6_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = (pjsip_status_code)jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  arg7 = *(pjsip_tx_data **)&jarg7; 
  arg8 = *(pjsip_rx_data **)&jarg8; 
  (arg1)->on_pager_status2(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,arg5,(pj_str_t const *)arg6,arg7,arg8);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1status2SwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jint jarg5, jlong jarg6, jobject jarg6_, jlong jarg7, jlong jarg8) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pjsip_status_code arg5 ;
  pj_str_t *arg6 = (pj_str_t *) 0 ;
  pjsip_tx_data *arg7 = (pjsip_tx_data *) 0 ;
  pjsip_rx_data *arg8 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg6_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = (pjsip_status_code)jarg5; 
  arg6 = *(pj_str_t **)&jarg6; 
  arg7 = *(pjsip_tx_data **)&jarg7; 
  arg8 = *(pjsip_rx_data **)&jarg8; 
  (arg1)->Callback::on_pager_status2(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,arg5,(pj_str_t const *)arg6,arg7,arg8);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1typing(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, jint jarg6) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  pj_bool_t arg6 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  arg6 = (pj_bool_t)jarg6; 
  (arg1)->on_typing(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pj_str_t const *)arg5,arg6);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1typingSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, jint jarg6) {
  Callback *arg1 = (Callback *) 0 ;
  pjsua_call_id arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_str_t *arg5 = (pj_str_t *) 0 ;
  pj_bool_t arg6 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pj_str_t **)&jarg5; 
  arg6 = (pj_bool_t)jarg6; 
  (arg1)->Callback::on_typing(arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pj_str_t const *)arg5,arg6);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1nat_1detect(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pj_stun_nat_detect_result *arg2 = (pj_stun_nat_detect_result *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = *(pj_stun_nat_detect_result **)&jarg2; 
  (arg1)->on_nat_detect((pj_stun_nat_detect_result const *)arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1nat_1detectSwigExplicitCallback(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  Callback *arg1 = (Callback *) 0 ;
  pj_stun_nat_detect_result *arg2 = (pj_stun_nat_detect_result *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  arg2 = *(pj_stun_nat_detect_result **)&jarg2; 
  (arg1)->Callback::on_nat_detect((pj_stun_nat_detect_result const *)arg2);
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1Callback(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  Callback *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (Callback *)new SwigDirector_Callback(jenv);
  *(Callback **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1director_1connect(JNIEnv *jenv, jclass jcls, jobject jself, jlong objarg, jboolean jswig_mem_own, jboolean jweak_global) {
  Callback *obj = *((Callback **)&objarg);
  (void)jcls;
  //TODO: reactivate
  //SwigDirector_Callback *director = dynamic_cast<SwigDirector_Callback *>(obj);
  SwigDirector_Callback *director = (SwigDirector_Callback*)obj;
  if (director) {
    director->swig_connect_director(jenv, jself, jenv->GetObjectClass(jself), (jswig_mem_own == JNI_TRUE), (jweak_global == JNI_TRUE));
  }
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_Callback_1change_1ownership(JNIEnv *jenv, jclass jcls, jobject jself, jlong objarg, jboolean jtake_or_release) {
  Callback *obj = *((Callback **)&objarg);
  //TODO: reactivate
  //SwigDirector_Callback *director = dynamic_cast<SwigDirector_Callback *>(obj);
  SwigDirector_Callback *director = (SwigDirector_Callback*)obj;
  (void)jcls;
  if (director) {
    director->swig_java_change_ownership(jenv, jself, jtake_or_release ? true : false);
  }
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_WRAPPER_1CALLBACK_1STRUCT_1get(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  struct pjsua_callback *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (struct pjsua_callback *) &wrapper_callback_struct;
  *(struct pjsua_callback **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_setCallbackObject(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  Callback *arg1 = (Callback *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(Callback **)&jarg1; 
  setCallbackObject(arg1);
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJ_1SUCCESS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 0;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJ_1TRUE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 1;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJ_1FALSE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 0;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1ptr_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  char *arg2 = (char *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg1->ptr) delete [] arg1->ptr;
    if (arg2) {
      arg1->ptr = (char *) (new char[strlen((const char *)arg2)+1]);
      strcpy((char *)arg1->ptr, (const char *)arg2);
    } else {
      arg1->ptr = 0;
    }
  }
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1ptr_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  result = (char *) ((arg1)->ptr);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1slen_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  pj_ssize_t arg2 ;
  pj_ssize_t *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  argp2 = *(pj_ssize_t **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_ssize_t");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->slen = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1slen_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  pj_ssize_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  result =  ((arg1)->slen);
  *(pj_ssize_t **)&jresult = new pj_ssize_t((const pj_ssize_t &)result); 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pj_1str_1t(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pj_str_t *)new pj_str_t();
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pj_1str_1t(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pj_str_t **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param *arg1 = (pjmedia_codec_param *) 0 ;
  pjmedia_codec_param_setting *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param **)&jarg1; 
  result = (pjmedia_codec_param_setting *)& ((arg1)->setting);
  *(pjmedia_codec_param_setting **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param *arg1 = (pjmedia_codec_param *) 0 ;
  pjmedia_codec_param_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param **)&jarg1; 
  result = (pjmedia_codec_param_info *)& ((arg1)->info);
  *(pjmedia_codec_param_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1codec_1param(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_codec_param *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_codec_param *)new pjmedia_codec_param();
  *(pjmedia_codec_param **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1codec_1param(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_codec_param *arg1 = (pjmedia_codec_param *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_codec_param **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1frm_1per_1pkt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  pj_uint8_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  if (arg1) (arg1)->frm_per_pkt = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1frm_1per_1pkt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  pj_uint8_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (pj_uint8_t) ((arg1)->frm_per_pkt);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1vad_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->vad = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1vad_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (unsigned int) ((arg1)->vad);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1cng_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->cng = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1cng_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (unsigned int) ((arg1)->cng);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1penh_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->penh = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1penh_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (unsigned int) ((arg1)->penh);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1plc_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->plc = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1plc_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (unsigned int) ((arg1)->plc);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1reserved_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->reserved = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1reserved_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (unsigned int) ((arg1)->reserved);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1enc_1fmtp_1mode_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  pj_uint8_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  if (arg1) (arg1)->enc_fmtp_mode = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1enc_1fmtp_1mode_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  pj_uint8_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (pj_uint8_t) ((arg1)->enc_fmtp_mode);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1dec_1fmtp_1mode_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  pj_uint8_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  if (arg1) (arg1)->dec_fmtp_mode = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1dec_1fmtp_1mode_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  pj_uint8_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  result = (pj_uint8_t) ((arg1)->dec_fmtp_mode);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1codec_1param_1setting(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_codec_param_setting *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_codec_param_setting *)new pjmedia_codec_param_setting();
  *(pjmedia_codec_param_setting **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1codec_1param_1setting(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_codec_param_setting *arg1 = (pjmedia_codec_param_setting *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_codec_param_setting **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1clock_1rate_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->clock_rate = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1clock_1rate_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (unsigned int) ((arg1)->clock_rate);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1channel_1cnt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->channel_cnt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1channel_1cnt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (unsigned int) ((arg1)->channel_cnt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1avg_1bps_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint32_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (pj_uint32_t)jarg2; 
  if (arg1) (arg1)->avg_bps = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1avg_1bps_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint32_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (pj_uint32_t) ((arg1)->avg_bps);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1max_1bps_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint32_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (pj_uint32_t)jarg2; 
  if (arg1) (arg1)->max_bps = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1max_1bps_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint32_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (pj_uint32_t) ((arg1)->max_bps);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1frm_1ptime_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint16_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (pj_uint16_t)jarg2; 
  if (arg1) (arg1)->frm_ptime = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1frm_1ptime_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint16_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (pj_uint16_t) ((arg1)->frm_ptime);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1enc_1ptime_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint16_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (pj_uint16_t)jarg2; 
  if (arg1) (arg1)->enc_ptime = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1enc_1ptime_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint16_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (pj_uint16_t) ((arg1)->enc_ptime);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pcm_1bits_1per_1sample_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint8_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  if (arg1) (arg1)->pcm_bits_per_sample = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pcm_1bits_1per_1sample_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint8_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (pj_uint8_t) ((arg1)->pcm_bits_per_sample);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint8_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  if (arg1) (arg1)->pt = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  pj_uint8_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  result = (pj_uint8_t) ((arg1)->pt);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1codec_1param_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_codec_param_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_codec_param_info *)new pjmedia_codec_param_info();
  *(pjmedia_codec_param_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1codec_1param_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_codec_param_info *arg1 = (pjmedia_codec_param_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_codec_param_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1NONE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjmedia_dir result;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_dir)PJMEDIA_DIR_NONE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1ENCODING_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjmedia_dir result;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_dir)PJMEDIA_DIR_ENCODING;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1DECODING_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjmedia_dir result;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_dir)PJMEDIA_DIR_DECODING;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1ENCODING_1DECODING_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjmedia_dir result;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_dir)PJMEDIA_DIR_ENCODING_DECODING;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->name = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->name);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1signature_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_uint32_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (pj_uint32_t)jarg2; 
  if (arg1) (arg1)->signature = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1signature_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_uint32_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (pj_uint32_t) ((arg1)->signature);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pjmedia_type arg2 ;
  pjmedia_type *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  argp2 = *(pjmedia_type **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjmedia_type");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->type = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pjmedia_type result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result =  ((arg1)->type);
  *(pjmedia_type **)&jresult = new pjmedia_type((const pjmedia_type &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1has_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->has_info = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1has_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (pj_bool_t) ((arg1)->has_info);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1need_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->need_info = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1need_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (pj_bool_t) ((arg1)->need_info);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1pt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->pt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1pt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->pt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1encoding_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->encoding_name = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1encoding_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->encoding_name);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1clock_1rate_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->clock_rate = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1clock_1rate_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->clock_rate);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1channel_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->channel_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1channel_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->channel_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bits_1per_1sample_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->bits_per_sample = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bits_1per_1sample_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->bits_per_sample);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1samples_1per_1frame_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->samples_per_frame = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1samples_1per_1frame_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->samples_per_frame);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bytes_1per_1frame_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->bytes_per_frame = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bytes_1per_1frame_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->bytes_per_frame);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1port_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_port_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_port_info *)new pjmedia_port_info();
  *(pjmedia_port_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1port_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_port_info *arg1 = (pjmedia_port_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_port_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pjmedia_port_info *arg2 = (pjmedia_port_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjmedia_port **)&jarg1; 
  arg2 = *(pjmedia_port_info **)&jarg2; 
  if (arg1) (arg1)->info = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pjmedia_port_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  result = (pjmedia_port_info *)& ((arg1)->info);
  *(pjmedia_port_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1put_1frame_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t (*arg2)(pjmedia_port *,pjmedia_frame const *) = (pj_status_t (*)(pjmedia_port *,pjmedia_frame const *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  arg2 = *(pj_status_t (**)(pjmedia_port *,pjmedia_frame const *))&jarg2; 
  if (arg1) (arg1)->put_frame = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1put_1frame_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t (*result)(pjmedia_port *,pjmedia_frame const *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  result = (pj_status_t (*)(pjmedia_port *,pjmedia_frame const *)) ((arg1)->put_frame);
  *(pj_status_t (**)(pjmedia_port *,pjmedia_frame const *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1get_1frame_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t (*arg2)(pjmedia_port *,pjmedia_frame *) = (pj_status_t (*)(pjmedia_port *,pjmedia_frame *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  arg2 = *(pj_status_t (**)(pjmedia_port *,pjmedia_frame *))&jarg2; 
  if (arg1) (arg1)->get_frame = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1get_1frame_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t (*result)(pjmedia_port *,pjmedia_frame *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  result = (pj_status_t (*)(pjmedia_port *,pjmedia_frame *)) ((arg1)->get_frame);
  *(pj_status_t (**)(pjmedia_port *,pjmedia_frame *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1on_1destroy_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t (*arg2)(pjmedia_port *) = (pj_status_t (*)(pjmedia_port *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  arg2 = *(pj_status_t (**)(pjmedia_port *))&jarg2; 
  if (arg1) (arg1)->on_destroy = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1on_1destroy_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t (*result)(pjmedia_port *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  result = (pj_status_t (*)(pjmedia_port *)) ((arg1)->on_destroy);
  *(pj_status_t (**)(pjmedia_port *))&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pjmedia_port_port_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  result = (pjmedia_port_port_data *)& ((arg1)->port_data);
  *(pjmedia_port_port_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1pdata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pjmedia_port_port_data *arg1 = (pjmedia_port_port_data *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_port_data **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->pdata = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1pdata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pjmedia_port_port_data *arg1 = (pjmedia_port_port_data *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_port_data **)&jarg1; 
  result = (void *) ((arg1)->pdata);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1ldata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjmedia_port_port_data *arg1 = (pjmedia_port_port_data *) 0 ;
  long arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_port_data **)&jarg1; 
  arg2 = (long)jarg2; 
  if (arg1) (arg1)->ldata = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1ldata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjmedia_port_port_data *arg1 = (pjmedia_port_port_data *) 0 ;
  long result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port_port_data **)&jarg1; 
  result = (long) ((arg1)->ldata);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1port_1port_1data(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_port_port_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_port_port_data *)new pjmedia_port_port_data();
  *(pjmedia_port_port_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1port_1port_1data(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_port_port_data *arg1 = (pjmedia_port_port_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_port_port_data **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1realm_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->realm = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1realm_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->realm);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1scheme_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->scheme = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1scheme_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->scheme);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1username_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->username = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1username_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->username);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->data_type = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  result = (int) ((arg1)->data_type);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->data = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->data);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  pjsip_cred_info_ext *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  result = (pjsip_cred_info_ext *)& ((arg1)->ext);
  *(pjsip_cred_info_ext **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1cred_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_cred_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_cred_info *)new pjsip_cred_info();
  *(pjsip_cred_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1cred_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_cred_info *arg1 = (pjsip_cred_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_cred_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext *arg1 = (pjsip_cred_info_ext *) 0 ;
  pjsip_cred_info_ext_aka *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info_ext **)&jarg1; 
  result = (pjsip_cred_info_ext_aka *)& ((arg1)->aka);
  *(pjsip_cred_info_ext_aka **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1cred_1info_1ext(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_cred_info_ext *)new pjsip_cred_info_ext();
  *(pjsip_cred_info_ext **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1cred_1info_1ext(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_cred_info_ext *arg1 = (pjsip_cred_info_ext *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_cred_info_ext **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1k_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->k = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1k_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->k);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1op_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->op = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1op_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->op);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1amf_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->amf = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1amf_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->amf);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1cb_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pjsip_cred_cb arg2 ;
  pjsip_cred_cb *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  argp2 = *(pjsip_cred_cb **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjsip_cred_cb");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->cb = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1cb_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  pjsip_cred_cb result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  result =  ((arg1)->cb);
  *(pjsip_cred_cb **)&jresult = new pjsip_cred_cb((const pjsip_cred_cb &)result); 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1cred_1info_1ext_1aka(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_cred_info_ext_aka *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_cred_info_ext_aka *)new pjsip_cred_info_ext_aka();
  *(pjsip_cred_info_ext_aka **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1cred_1info_1ext_1aka(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_cred_info_ext_aka *arg1 = (pjsip_cred_info_ext_aka *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_cred_info_ext_aka **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1CRED_1DATA_1PLAIN_1PASSWD_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_cred_data_type result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_cred_data_type)PJSIP_CRED_DATA_PLAIN_PASSWD;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1CRED_1DATA_1DIGEST_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_cred_data_type result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_cred_data_type)PJSIP_CRED_DATA_DIGEST;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1CRED_1DATA_1EXT_1AKA_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_cred_data_type result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_cred_data_type)PJSIP_CRED_DATA_EXT_AKA;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1TONEGEN_1LOOP_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int)PJMEDIA_TONEGEN_LOOP;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1TONEGEN_1NO_1LOCK_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int)PJMEDIA_TONEGEN_NO_LOCK;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsip_event *arg1 = (pjsip_event *) 0 ;
  pjsip_event_id_e arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event **)&jarg1; 
  arg2 = (pjsip_event_id_e)jarg2; 
  if (arg1) (arg1)->type = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsip_event *arg1 = (pjsip_event *) 0 ;
  pjsip_event_id_e result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event **)&jarg1; 
  result = (pjsip_event_id_e) ((arg1)->type);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event *arg1 = (pjsip_event *) 0 ;
  pjsip_event_body *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event **)&jarg1; 
  result = (pjsip_event_body *)& ((arg1)->body);
  *(pjsip_event_body **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event *)new pjsip_event();
  *(pjsip_event **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event *arg1 = (pjsip_event *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  pjsip_event_body_user *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body **)&jarg1; 
  result = (pjsip_event_body_user *)& ((arg1)->user);
  *(pjsip_event_body_user **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1rx_1msg_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  pjsip_event_body_rx_msg *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body **)&jarg1; 
  result = (pjsip_event_body_rx_msg *)& ((arg1)->rx_msg);
  *(pjsip_event_body_rx_msg **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  pjsip_event_body_tx_error *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body **)&jarg1; 
  result = (pjsip_event_body_tx_error *)& ((arg1)->tx_error);
  *(pjsip_event_body_tx_error **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1msg_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  pjsip_event_body_tx_msg *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body **)&jarg1; 
  result = (pjsip_event_body_tx_msg *)& ((arg1)->tx_msg);
  *(pjsip_event_body_tx_msg **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  pjsip_event_body_tsx_state *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body **)&jarg1; 
  result = (pjsip_event_body_tsx_state *)& ((arg1)->tsx_state);
  *(pjsip_event_body_tsx_state **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1timer_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  pjsip_event_body_timer *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body **)&jarg1; 
  result = (pjsip_event_body_timer *)& ((arg1)->timer);
  *(pjsip_event_body_timer **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body *)new pjsip_event_body();
  *(pjsip_event_body **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body *arg1 = (pjsip_event_body *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user1_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->user1 = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user1_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  result = (void *) ((arg1)->user1);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user2_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->user2 = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user2_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  result = (void *) ((arg1)->user2);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user3_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->user3 = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user3_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  result = (void *) ((arg1)->user3);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user4_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->user4 = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user4_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  result = (void *) ((arg1)->user4);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1user(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_user *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_user *)new pjsip_event_body_user();
  *(pjsip_event_body_user **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1user(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_user *arg1 = (pjsip_event_body_user *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_user **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1rx_1msg_1rdata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_rx_msg *arg1 = (pjsip_event_body_rx_msg *) 0 ;
  pjsip_rx_data *arg2 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_rx_msg **)&jarg1; 
  arg2 = *(pjsip_rx_data **)&jarg2; 
  if (arg1) (arg1)->rdata = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1rx_1msg_1rdata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_rx_msg *arg1 = (pjsip_event_body_rx_msg *) 0 ;
  pjsip_rx_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_rx_msg **)&jarg1; 
  result = (pjsip_rx_data *) ((arg1)->rdata);
  *(pjsip_rx_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1rx_1msg(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_rx_msg *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_rx_msg *)new pjsip_event_body_rx_msg();
  *(pjsip_event_body_rx_msg **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1rx_1msg(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_rx_msg *arg1 = (pjsip_event_body_rx_msg *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_rx_msg **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tdata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tx_error *arg1 = (pjsip_event_body_tx_error *) 0 ;
  pjsip_tx_data *arg2 = (pjsip_tx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tx_error **)&jarg1; 
  arg2 = *(pjsip_tx_data **)&jarg2; 
  if (arg1) (arg1)->tdata = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tdata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tx_error *arg1 = (pjsip_event_body_tx_error *) 0 ;
  pjsip_tx_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tx_error **)&jarg1; 
  result = (pjsip_tx_data *) ((arg1)->tdata);
  *(pjsip_tx_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tsx_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tx_error *arg1 = (pjsip_event_body_tx_error *) 0 ;
  pjsip_transaction *arg2 = (pjsip_transaction *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tx_error **)&jarg1; 
  arg2 = *(pjsip_transaction **)&jarg2; 
  if (arg1) (arg1)->tsx = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tsx_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tx_error *arg1 = (pjsip_event_body_tx_error *) 0 ;
  pjsip_transaction *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tx_error **)&jarg1; 
  result = (pjsip_transaction *) ((arg1)->tsx);
  *(pjsip_transaction **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tx_1error(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_tx_error *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_tx_error *)new pjsip_event_body_tx_error();
  *(pjsip_event_body_tx_error **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tx_1error(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_tx_error *arg1 = (pjsip_event_body_tx_error *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_tx_error **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1msg_1tdata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tx_msg *arg1 = (pjsip_event_body_tx_msg *) 0 ;
  pjsip_tx_data *arg2 = (pjsip_tx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tx_msg **)&jarg1; 
  arg2 = *(pjsip_tx_data **)&jarg2; 
  if (arg1) (arg1)->tdata = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1msg_1tdata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tx_msg *arg1 = (pjsip_event_body_tx_msg *) 0 ;
  pjsip_tx_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tx_msg **)&jarg1; 
  result = (pjsip_tx_data *) ((arg1)->tdata);
  *(pjsip_tx_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tx_1msg(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_tx_msg *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_tx_msg *)new pjsip_event_body_tx_msg();
  *(pjsip_event_body_tx_msg **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tx_1msg(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_tx_msg *arg1 = (pjsip_event_body_tx_msg *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_tx_msg **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1tsx_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  pjsip_transaction *arg2 = (pjsip_transaction *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  arg2 = *(pjsip_transaction **)&jarg2; 
  if (arg1) (arg1)->tsx = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1tsx_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  pjsip_transaction *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  result = (pjsip_transaction *) ((arg1)->tsx);
  *(pjsip_transaction **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1prev_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->prev_state = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1prev_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  result = (int) ((arg1)->prev_state);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  pjsip_event_id_e arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  arg2 = (pjsip_event_id_e)jarg2; 
  if (arg1) (arg1)->type = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  pjsip_event_id_e result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  result = (pjsip_event_id_e) ((arg1)->type);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  pjsip_event_body_tsx_state_src *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  result = (pjsip_event_body_tsx_state_src *)& ((arg1)->src);
  *(pjsip_event_body_tsx_state_src **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tsx_1state(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_tsx_state *)new pjsip_event_body_tsx_state();
  *(pjsip_event_body_tsx_state **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tsx_1state(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_tsx_state *arg1 = (pjsip_event_body_tsx_state *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_tsx_state **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1rdata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pjsip_rx_data *arg2 = (pjsip_rx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  arg2 = *(pjsip_rx_data **)&jarg2; 
  if (arg1) (arg1)->rdata = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1rdata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pjsip_rx_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  result = (pjsip_rx_data *) ((arg1)->rdata);
  *(pjsip_rx_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1tdata_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pjsip_tx_data *arg2 = (pjsip_tx_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  arg2 = *(pjsip_tx_data **)&jarg2; 
  if (arg1) (arg1)->tdata = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1tdata_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pjsip_tx_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  result = (pjsip_tx_data *) ((arg1)->tdata);
  *(pjsip_tx_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1timer_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pj_timer_entry *arg2 = (pj_timer_entry *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  arg2 = *(pj_timer_entry **)&jarg2; 
  if (arg1) (arg1)->timer = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1timer_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pj_timer_entry *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  result = (pj_timer_entry *) ((arg1)->timer);
  *(pj_timer_entry **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pj_status_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  arg2 = (pj_status_t)jarg2; 
  if (arg1) (arg1)->status = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  result = (pj_status_t) ((arg1)->status);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1data_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->data = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1data_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  result = (void *) ((arg1)->data);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tsx_1state_1src(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_tsx_state_src *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_tsx_state_src *)new pjsip_event_body_tsx_state_src();
  *(pjsip_event_body_tsx_state_src **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tsx_1state_1src(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_tsx_state_src *arg1 = (pjsip_event_body_tsx_state_src *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_tsx_state_src **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1timer_1entry_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsip_event_body_timer *arg1 = (pjsip_event_body_timer *) 0 ;
  pj_timer_entry *arg2 = (pj_timer_entry *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_timer **)&jarg1; 
  arg2 = *(pj_timer_entry **)&jarg2; 
  if (arg1) (arg1)->entry = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1timer_1entry_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsip_event_body_timer *arg1 = (pjsip_event_body_timer *) 0 ;
  pj_timer_entry *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsip_event_body_timer **)&jarg1; 
  result = (pj_timer_entry *) ((arg1)->entry);
  *(pj_timer_entry **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1timer(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_event_body_timer *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_event_body_timer *)new pjsip_event_body_timer();
  *(pjsip_event_body_timer **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1timer(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsip_event_body_timer *arg1 = (pjsip_event_body_timer *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_event_body_timer **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->name, (const char *)arg2, 64);
    else arg1->name[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  result = (char *)(char *) ((arg1)->name);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1input_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->input_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1input_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  result = (unsigned int) ((arg1)->input_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1output_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->output_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1output_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  result = (unsigned int) ((arg1)->output_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1default_1samples_1per_1sec_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->default_samples_per_sec = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1default_1samples_1per_1sec_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  result = (unsigned int) ((arg1)->default_samples_per_sec);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1snd_1dev_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_snd_dev_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_snd_dev_info *)new pjmedia_snd_dev_info();
  *(pjmedia_snd_dev_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1snd_1dev_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_snd_dev_info *arg1 = (pjmedia_snd_dev_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq1_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  arg2 = (short)jarg2; 
  if (arg1) (arg1)->freq1 = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq1_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  result = (short) ((arg1)->freq1);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq2_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  arg2 = (short)jarg2; 
  if (arg1) (arg1)->freq2 = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq2_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  result = (short) ((arg1)->freq2);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1on_1msec_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  arg2 = (short)jarg2; 
  if (arg1) (arg1)->on_msec = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1on_1msec_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  result = (short) ((arg1)->on_msec);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1off_1msec_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  arg2 = (short)jarg2; 
  if (arg1) (arg1)->off_msec = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1off_1msec_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  result = (short) ((arg1)->off_msec);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1volume_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  arg2 = (short)jarg2; 
  if (arg1) (arg1)->volume = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1volume_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  short result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  result = (short) ((arg1)->volume);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1tone_1desc(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_tone_desc *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_tone_desc *)new pjmedia_tone_desc();
  *(pjmedia_tone_desc **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1tone_1desc(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjmedia_tone_desc *arg1 = (pjmedia_tone_desc *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjmedia_tone_desc **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1obj_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->obj_name, (const char *)arg2, PJ_MAX_OBJ_NAME);
    else arg1->obj_name[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1obj_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result = (char *)(char *) ((arg1)->obj_name);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_pool_factory *arg2 = (pj_pool_factory *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pj_pool_factory **)&jarg2; 
  if (arg1) (arg1)->factory = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_pool_factory *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result = (pj_pool_factory *) ((arg1)->factory);
  *(pj_pool_factory **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1data_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, void * jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  void *arg2 = (void *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  
  arg2 = jarg2;
  
  if (arg1) (arg1)->factory_data = arg2;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1data_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  void * jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result = (void *) ((arg1)->factory_data);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1capacity_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_size_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = (pj_size_t)jarg2; 
  if (arg1) (arg1)->capacity = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1capacity_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_size_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result =  ((arg1)->capacity);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1increment_1size_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_size_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = (pj_size_t)jarg2; 
  if (arg1) (arg1)->increment_size = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1increment_1size_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_size_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result =  ((arg1)->increment_size);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1block_1list_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_pool_block arg2 ;
  pj_pool_block *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  argp2 = *(pj_pool_block **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_pool_block");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->block_list = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1block_1list_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_pool_block result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result =  ((arg1)->block_list);
  *(pj_pool_block **)&jresult = new pj_pool_block((const pj_pool_block &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1callback_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_pool_callback *arg2 = (pj_pool_callback *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pj_pool_callback **)&jarg2; 
  if (arg1) (arg1)->callback = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1callback_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_pool_callback *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  result = (pj_pool_callback *) ((arg1)->callback);
  *(pj_pool_callback **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pj_1pool_1t(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pj_pool_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pj_pool_t *)new pj_pool_t();
  *(pj_pool_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pj_1pool_1t(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pj_pool_t **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1TRANSPORT_1IPV6_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_transport_type_e result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_transport_type_e)PJSIP_TRANSPORT_IPV6;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1TRANSPORT_1UDP6_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_transport_type_e result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_transport_type_e)PJSIP_TRANSPORT_UDP6;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1TRANSPORT_1TCP6_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_transport_type_e result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_transport_type_e)PJSIP_TRANSPORT_TCP6;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TRYING_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_TRYING;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1RINGING_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_RINGING;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1CALL_1BEING_1FORWARDED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_CALL_BEING_FORWARDED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1QUEUED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_QUEUED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PROGRESS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_PROGRESS;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1OK_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_OK;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1ACCEPTED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_ACCEPTED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MULTIPLE_1CHOICES_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_MULTIPLE_CHOICES;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MOVED_1PERMANENTLY_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_MOVED_PERMANENTLY;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MOVED_1TEMPORARILY_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_MOVED_TEMPORARILY;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1USE_1PROXY_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_USE_PROXY;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1ALTERNATIVE_1SERVICE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_ALTERNATIVE_SERVICE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1REQUEST_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_BAD_REQUEST;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNAUTHORIZED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_UNAUTHORIZED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PAYMENT_1REQUIRED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_PAYMENT_REQUIRED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1FORBIDDEN_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_FORBIDDEN;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1FOUND_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_NOT_FOUND;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1METHOD_1NOT_1ALLOWED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_METHOD_NOT_ALLOWED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1ACCEPTABLE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_NOT_ACCEPTABLE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PROXY_1AUTHENTICATION_1REQUIRED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1TIMEOUT_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_REQUEST_TIMEOUT;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1GONE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_GONE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1ENTITY_1TOO_1LARGE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_REQUEST_ENTITY_TOO_LARGE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1URI_1TOO_1LONG_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_REQUEST_URI_TOO_LONG;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNSUPPORTED_1MEDIA_1TYPE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_UNSUPPORTED_MEDIA_TYPE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNSUPPORTED_1URI_1SCHEME_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_UNSUPPORTED_URI_SCHEME;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1EXTENSION_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_BAD_EXTENSION;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1EXTENSION_1REQUIRED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_EXTENSION_REQUIRED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1SESSION_1TIMER_1TOO_1SMALL_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_SESSION_TIMER_TOO_SMALL;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1INTERVAL_1TOO_1BRIEF_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_INTERVAL_TOO_BRIEF;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TEMPORARILY_1UNAVAILABLE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_TEMPORARILY_UNAVAILABLE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1CALL_1TSX_1DOES_1NOT_1EXIST_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_CALL_TSX_DOES_NOT_EXIST;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1LOOP_1DETECTED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_LOOP_DETECTED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TOO_1MANY_1HOPS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_TOO_MANY_HOPS;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1ADDRESS_1INCOMPLETE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_ADDRESS_INCOMPLETE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1AC_1AMBIGUOUS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_AC_AMBIGUOUS;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BUSY_1HERE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_BUSY_HERE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1TERMINATED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_REQUEST_TERMINATED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1ACCEPTABLE_1HERE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_NOT_ACCEPTABLE_HERE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1EVENT_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_BAD_EVENT;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1UPDATED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_REQUEST_UPDATED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1PENDING_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_REQUEST_PENDING;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNDECIPHERABLE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_UNDECIPHERABLE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1INTERNAL_1SERVER_1ERROR_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_INTERNAL_SERVER_ERROR;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1IMPLEMENTED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_NOT_IMPLEMENTED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1GATEWAY_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_BAD_GATEWAY;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1SERVICE_1UNAVAILABLE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_SERVICE_UNAVAILABLE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1SERVER_1TIMEOUT_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_SERVER_TIMEOUT;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1VERSION_1NOT_1SUPPORTED_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_VERSION_NOT_SUPPORTED;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MESSAGE_1TOO_1LARGE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_MESSAGE_TOO_LARGE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PRECONDITION_1FAILURE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_PRECONDITION_FAILURE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BUSY_1EVERYWHERE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_BUSY_EVERYWHERE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1DECLINE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_DECLINE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1DOES_1NOT_1EXIST_1ANYWHERE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_DOES_NOT_EXIST_ANYWHERE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1ACCEPTABLE_1ANYWHERE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TSX_1TIMEOUT_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_TSX_TIMEOUT;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TSX_1TRANSPORT_1ERROR_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_status_code)PJSIP_SC_TSX_TRANSPORT_ERROR;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1pool_1create(JNIEnv *jenv, jclass jcls, jstring jarg1, jlong jarg2, jlong jarg3) {
  jlong jresult = 0 ;
  char *arg1 = (char *) 0 ;
  pj_size_t arg2 ;
  pj_size_t arg3 ;
  pj_pool_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = 0;
  if (jarg1) {
    arg1 = (char *)jenv->GetStringUTFChars(jarg1, 0);
    if (!arg1) return 0;
  }
  arg2 = (pj_size_t)jarg2; 
  arg3 = (pj_size_t)jarg3; 
  result = (pj_pool_t *)pjsua_pool_create((char const *)arg1,arg2,arg3);
  *(pj_pool_t **)&jresult = result; 
  if (arg1) jenv->ReleaseStringUTFChars(jarg1, (const char *)arg1);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1release(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_pool_t **)&jarg1; 
  pj_pool_release(arg1);
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_snd_1get_1dev_1count(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int)pjmedia_aud_dev_count();
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1set_1latency(JNIEnv *jenv, jclass jcls, jlong jarg1, jlong jarg2) {
  jint jresult = 0 ;
  unsigned int arg1 ;
  unsigned int arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (unsigned int)jarg1; 
  arg2 = (unsigned int)jarg2; 
  //TODO: reimplement
  //result = (pj_status_t)pjmedia_snd_set_latency(arg1,arg2);
  result = 0;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tonegen_1create2(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jlong jarg3, jlong jarg4, jlong jarg5, jlong jarg6, jlong jarg7, jobject jarg8) {
  jint jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  unsigned int arg3 ;
  unsigned int arg4 ;
  unsigned int arg5 ;
  unsigned int arg6 ;
  unsigned int arg7 ;
  pjmedia_port **arg8 = (pjmedia_port **) 0 ;
  pjmedia_port *ppMediaPort8 = 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = (unsigned int)jarg3; 
  arg4 = (unsigned int)jarg4; 
  arg5 = (unsigned int)jarg5; 
  arg6 = (unsigned int)jarg6; 
  arg7 = (unsigned int)jarg7; 
  
  arg8 = &ppMediaPort8;
  
  result = (pj_status_t)pjmedia_tonegen_create2(arg1,(pj_str_t const *)arg2,arg3,arg4,arg5,arg6,arg7,arg8);
  jresult = (jint)result; 
  {
    // Give Java proxy the C pointer (of newly created object)
    jclass clazz = jenv->FindClass("org/pjsip/pjsua/pjmedia_port");
    jfieldID fid = jenv->GetFieldID(clazz, "swigCPtr", "J");
    jlong cPtr = 0;
    *(pjmedia_port **)&cPtr = *arg8;
    jenv->SetLongField(jarg8, fid, cPtr);
  }
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tonegen_1play(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jlongArray jarg3, jlong jarg4) {
  jint jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  unsigned int arg2 ;
  pjmedia_tone_desc *arg3 ;
  unsigned int arg4 ;
  jlong *jarr3 ;
  jsize sz3 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  {
    int i;
    if (!jarg3) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null array");
      return 0;
    }
    sz3 = jenv->GetArrayLength(jarg3);
    jarr3 = jenv->GetLongArrayElements(jarg3, 0);
    if (!jarr3) {
      return 0;
    }
    
    arg3 = new pjmedia_tone_desc[sz3];
    
    
    
    if (!arg3) {
      SWIG_JavaThrowException(jenv, SWIG_JavaOutOfMemoryError, "array memory allocation failed");
      return 0;
    }
    for (i=0; i<sz3; i++) {
      arg3[i] = **(pjmedia_tone_desc **)&jarr3[i];
    }
  }
  arg4 = (unsigned int)jarg4; 
  result = (pj_status_t)pjmedia_tonegen_play(arg1,arg2,(pjmedia_tone_desc const (*))arg3,arg4);
  jresult = (jint)result; 
  {
    int i;
    for (i=0; i<sz3; i++) {
      **(pjmedia_tone_desc **)&jarr3[i] = arg3[i];
    }
    jenv->ReleaseLongArrayElements(jarg3, jarr3, 0);
  }
  delete [] arg3; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tonegen_1rewind(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjmedia_port *arg1 = (pjmedia_port *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_port **)&jarg1; 
  result = (pj_status_t)pjmedia_tonegen_rewind(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1INVALID_1ID_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) (-1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1ACC_1MAX_1PROXIES_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 8;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1msg_1logging_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->msg_logging = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1msg_1logging_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->msg_logging);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1level_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->level = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1level_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (unsigned int) ((arg1)->level);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1console_1level_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->console_level = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1console_1level_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (unsigned int) ((arg1)->console_level);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1decor_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->decor = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1decor_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (unsigned int) ((arg1)->decor);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1log_1filename_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->log_filename = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1log_1filename_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->log_filename);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1cb_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  void (*arg2)(int,char const *,int) = (void (*)(int,char const *,int)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  arg2 = *(void (**)(int,char const *,int))&jarg2; 
  if (arg1) (arg1)->cb = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1cb_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  void (*result)(int,char const *,int) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (void (*)(int,char const *,int)) ((arg1)->cb);
  *(void (**)(int,char const *,int))&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1logging_1config(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_logging_config *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_logging_config *)new pjsua_logging_config();
  *(pjsua_logging_config **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1logging_1config(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_logging_1config_1default(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  pjsua_logging_config_default(arg1);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_logging_1config_1dup(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pjsua_logging_config *arg2 = (pjsua_logging_config *) 0 ;
  pjsua_logging_config *arg3 = (pjsua_logging_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  (void)jarg3_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pjsua_logging_config **)&jarg2; 
  arg3 = *(pjsua_logging_config **)&jarg3; 
  pjsua_logging_config_dup(arg1,arg2,(pjsua_logging_config const *)arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pjsip_event *) = (void (*)(pjsua_call_id,pjsip_event *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pjsip_event *))&jarg2; 
  if (arg1) (arg1)->on_call_state = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pjsip_event *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pjsip_event *)) ((arg1)->on_call_state);
  *(void (**)(pjsua_call_id,pjsip_event *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1incoming_1call_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_acc_id,pjsua_call_id,pjsip_rx_data *) = (void (*)(pjsua_acc_id,pjsua_call_id,pjsip_rx_data *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_acc_id,pjsua_call_id,pjsip_rx_data *))&jarg2; 
  if (arg1) (arg1)->on_incoming_call = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1incoming_1call_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_acc_id,pjsua_call_id,pjsip_rx_data *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_acc_id,pjsua_call_id,pjsip_rx_data *)) ((arg1)->on_incoming_call);
  *(void (**)(pjsua_acc_id,pjsua_call_id,pjsip_rx_data *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1tsx_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pjsip_transaction *,pjsip_event *) = (void (*)(pjsua_call_id,pjsip_transaction *,pjsip_event *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pjsip_transaction *,pjsip_event *))&jarg2; 
  if (arg1) (arg1)->on_call_tsx_state = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1tsx_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pjsip_transaction *,pjsip_event *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pjsip_transaction *,pjsip_event *)) ((arg1)->on_call_tsx_state);
  *(void (**)(pjsua_call_id,pjsip_transaction *,pjsip_event *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1media_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id) = (void (*)(pjsua_call_id)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id))&jarg2; 
  if (arg1) (arg1)->on_call_media_state = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1media_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id)) ((arg1)->on_call_media_state);
  *(void (**)(pjsua_call_id))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1created_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pjmedia_session *,unsigned int,pjmedia_port **) = (void (*)(pjsua_call_id,pjmedia_session *,unsigned int,pjmedia_port **)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pjmedia_session *,unsigned int,pjmedia_port **))&jarg2;
  if (arg1) (arg1)->on_stream_created = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1created_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pjmedia_session *,unsigned int,pjmedia_port **) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pjmedia_session *,unsigned int,pjmedia_port **)) ((arg1)->on_stream_created);
  *(void (**)(pjsua_call_id,pjmedia_session *,unsigned int,pjmedia_port **))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1destroyed_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pjmedia_session *,unsigned int) = (void (*)(pjsua_call_id,pjmedia_session *,unsigned int)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pjmedia_session *,unsigned int))&jarg2; 
  if (arg1) (arg1)->on_stream_destroyed = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1destroyed_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pjmedia_session *,unsigned int) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pjmedia_session *,unsigned int)) ((arg1)->on_stream_destroyed);
  *(void (**)(pjsua_call_id,pjmedia_session *,unsigned int))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1dtmf_1digit_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,int) = (void (*)(pjsua_call_id,int)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,int))&jarg2; 
  if (arg1) (arg1)->on_dtmf_digit = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1dtmf_1digit_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,int) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,int)) ((arg1)->on_dtmf_digit);
  *(void (**)(pjsua_call_id,int))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1request_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pj_str_t const *,pjsip_status_code *) = (void (*)(pjsua_call_id,pj_str_t const *,pjsip_status_code *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pj_str_t const *,pjsip_status_code *))&jarg2; 
  if (arg1) (arg1)->on_call_transfer_request = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1request_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pj_str_t const *,pjsip_status_code *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pj_str_t const *,pjsip_status_code *)) ((arg1)->on_call_transfer_request);
  *(void (**)(pjsua_call_id,pj_str_t const *,pjsip_status_code *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,int,pj_str_t const *,pj_bool_t,pj_bool_t *) = (void (*)(pjsua_call_id,int,pj_str_t const *,pj_bool_t,pj_bool_t *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,int,pj_str_t const *,pj_bool_t,pj_bool_t *))&jarg2; 
  if (arg1) (arg1)->on_call_transfer_status = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,int,pj_str_t const *,pj_bool_t,pj_bool_t *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,int,pj_str_t const *,pj_bool_t,pj_bool_t *)) ((arg1)->on_call_transfer_status);
  *(void (**)(pjsua_call_id,int,pj_str_t const *,pj_bool_t,pj_bool_t *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replace_1request_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pjsip_rx_data *,int *,pj_str_t *) = (void (*)(pjsua_call_id,pjsip_rx_data *,int *,pj_str_t *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pjsip_rx_data *,int *,pj_str_t *))&jarg2; 
  if (arg1) (arg1)->on_call_replace_request = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replace_1request_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pjsip_rx_data *,int *,pj_str_t *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pjsip_rx_data *,int *,pj_str_t *)) ((arg1)->on_call_replace_request);
  *(void (**)(pjsua_call_id,pjsip_rx_data *,int *,pj_str_t *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replaced_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pjsua_call_id) = (void (*)(pjsua_call_id,pjsua_call_id)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pjsua_call_id))&jarg2; 
  if (arg1) (arg1)->on_call_replaced = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replaced_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pjsua_call_id) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pjsua_call_id)) ((arg1)->on_call_replaced);
  *(void (**)(pjsua_call_id,pjsua_call_id))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1reg_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_acc_id) = (void (*)(pjsua_acc_id)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_acc_id))&jarg2; 
  if (arg1) (arg1)->on_reg_state = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1reg_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_acc_id) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_acc_id)) ((arg1)->on_reg_state);
  *(void (**)(pjsua_acc_id))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1buddy_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_buddy_id) = (void (*)(pjsua_buddy_id)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_buddy_id))&jarg2; 
  if (arg1) (arg1)->on_buddy_state = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1buddy_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_buddy_id) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_buddy_id)) ((arg1)->on_buddy_state);
  *(void (**)(pjsua_buddy_id))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *) = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *))&jarg2; 
  if (arg1) (arg1)->on_pager = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *)) ((arg1)->on_pager);
  *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager2_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pjsip_rx_data *) = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pjsip_rx_data *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  //arg1 = *(pjsua_callback **)&jarg1;
  //arg2 = *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pjsip_rx_data *))&jarg2;
  //TODO: reactivate
  //if (arg1) (arg1)->on_pager2 = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager2_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pjsip_rx_data *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pjsip_rx_data *)) ((arg1)->on_pager2);
  *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_str_t const *,pjsip_rx_data *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *) = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *))&jarg2; 
  if (arg1) (arg1)->on_pager_status = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *)) ((arg1)->on_pager_status);
  *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status2_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *,pjsip_tx_data *,pjsip_rx_data *) = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *,pjsip_tx_data *,pjsip_rx_data *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  //TODO : reactivate
  //arg1 = *(pjsua_callback **)&jarg1;
  //arg2 = *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *,pjsip_tx_data *,pjsip_rx_data *))&jarg2;
  //if (arg1) (arg1)->on_pager_status2 = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status2_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *,pjsip_tx_data *,pjsip_rx_data *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *,pjsip_tx_data *,pjsip_rx_data *)) ((arg1)->on_pager_status2);
  *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,void *,pjsip_status_code,pj_str_t const *,pjsip_tx_data *,pjsip_rx_data *))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1typing_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_bool_t) = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_bool_t)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_bool_t))&jarg2; 
  if (arg1) (arg1)->on_typing = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1typing_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_bool_t) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_bool_t)) ((arg1)->on_typing);
  *(void (**)(pjsua_call_id,pj_str_t const *,pj_str_t const *,pj_str_t const *,pj_bool_t))&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1nat_1detect_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*arg2)(pj_stun_nat_detect_result const *) = (void (*)(pj_stun_nat_detect_result const *)) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  arg2 = *(void (**)(pj_stun_nat_detect_result const *))&jarg2; 
  if (arg1) (arg1)->on_nat_detect = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1nat_1detect_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  void (*result)(pj_stun_nat_detect_result const *) = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_callback **)&jarg1; 
  result = (void (*)(pj_stun_nat_detect_result const *)) ((arg1)->on_nat_detect);
  *(void (**)(pj_stun_nat_detect_result const *))&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1callback(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_callback *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_callback *)new pjsua_callback();
  *(pjsua_callback **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1callback(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_callback *arg1 = (pjsua_callback *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_callback **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1max_1calls_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->max_calls = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1max_1calls_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (unsigned int) ((arg1)->max_calls);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1thread_1cnt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->thread_cnt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1thread_1cnt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (unsigned int) ((arg1)->thread_cnt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->nameserver_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (unsigned int) ((arg1)->nameserver_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  {
    size_t ii;
    pj_str_t *b = (pj_str_t *) arg1->nameserver;
    for (ii = 0; ii < (size_t)4; ii++) b[ii] = *((pj_str_t *) arg2 + ii);
  }
  
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pj_str_t *)(pj_str_t *) ((arg1)->nameserver);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1cnt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->outbound_proxy_cnt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1cnt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (unsigned int) ((arg1)->outbound_proxy_cnt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  {
    size_t ii;
    pj_str_t *b = (pj_str_t *) arg1->outbound_proxy;
    for (ii = 0; ii < (size_t)4; ii++) b[ii] = *((pj_str_t *) arg2 + ii);
  }
  
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pj_str_t *)(pj_str_t *) ((arg1)->outbound_proxy);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1domain_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->stun_domain = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1domain_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->stun_domain);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1host_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->stun_host = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1host_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->stun_host);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nat_1type_1in_1sdp_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->nat_type_in_sdp = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nat_1type_1in_1sdp_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (int) ((arg1)->nat_type_in_sdp);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1require_1100rel_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->require_100rel = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1require_1100rel_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->require_100rel);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->cred_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (unsigned int) ((arg1)->cred_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pjsip_cred_info *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pjsip_cred_info **)&jarg2; 
  {
    size_t ii;
    pjsip_cred_info *b = (pjsip_cred_info *) arg1->cred_info;
    for (ii = 0; ii < (size_t)8; ii++) b[ii] = *((pjsip_cred_info *) arg2 + ii);
  }
  
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pjsip_cred_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pjsip_cred_info *)(pjsip_cred_info *) ((arg1)->cred_info);
  *(pjsip_cred_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cb_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pjsua_callback *arg2 = (pjsua_callback *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pjsua_callback **)&jarg2; 
  if (arg1) (arg1)->cb = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cb_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pjsua_callback *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pjsua_callback *)& ((arg1)->cb);
  *(pjsua_callback **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1user_1agent_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->user_agent = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1user_1agent_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->user_agent);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1config(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_config *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_config *)new pjsua_config();
  *(pjsua_config **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1config(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_config **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_config_1default(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_config *arg1 = (pjsua_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_config **)&jarg1; 
  pjsua_config_default(arg1);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_config_1dup(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pjsua_config *arg2 = (pjsua_config *) 0 ;
  pjsua_config *arg3 = (pjsua_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  (void)jarg3_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pjsua_config **)&jarg2; 
  arg3 = *(pjsua_config **)&jarg3; 
  pjsua_config_dup(arg1,arg2,(pjsua_config const *)arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1hdr_1list_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  pjsip_hdr arg2 ;
  pjsip_hdr *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  argp2 = *(pjsip_hdr **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjsip_hdr");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->hdr_list = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1hdr_1list_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  pjsip_hdr result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  result =  ((arg1)->hdr_list);
  *(pjsip_hdr **)&jresult = new pjsip_hdr((const pjsip_hdr &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1content_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->content_type = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1content_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->content_type);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1msg_1body_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->msg_body = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1msg_1body_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->msg_body);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1msg_1data(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_msg_data *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_msg_data *)new pjsua_msg_data();
  *(pjsua_msg_data **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1msg_1data(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_msg_1data_1init(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_msg_data *arg1 = (pjsua_msg_data *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_msg_data **)&jarg1; 
  pjsua_msg_data_init(arg1);
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_create(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pj_status_t result;

  (void)jenv;
  (void)jcls;
  result = (pj_status_t)pjsua_create();
  jresult = (jint)result; 
  return jresult;
}



SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_init(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
	jint jresult = 0 ;
	pjsua_config *arg1 = (pjsua_config *) 0 ;
	pjsua_logging_config *arg2 = (pjsua_logging_config *) 0 ;
	pjsua_media_config *arg3 = (pjsua_media_config *) 0 ;
	pj_status_t result;

	(void)jenv;
	(void)jcls;
	(void)jarg1_;
	(void)jarg2_;
	(void)jarg3_;
	arg1 = *(pjsua_config **)&jarg1;
	arg2 = *(pjsua_logging_config **)&jarg2;
	arg3 = *(pjsua_media_config **)&jarg3;

	arg2->cb = &on_log_msg;
	result = (pj_status_t)pjsua_init((pjsua_config const *)arg1,(pjsua_logging_config const *)arg2,(pjsua_media_config const *)arg3);
	jresult = (jint)result;
	if(result == PJ_SUCCESS){
		init_ringback_tone();
	}
	return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_start(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  result = (pj_status_t)pjsua_start();
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_destroy(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pj_status_t result;
  (void)jenv;
  (void)jcls;
  destroy_ringback_tone();
  result = (pj_status_t)pjsua_destroy();
  jresult = (jint)result;
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_handle_1events(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  jint jresult = 0 ;
  unsigned int arg1 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (unsigned int)jarg1; 
  result = (int)pjsua_handle_events(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pool_1create(JNIEnv *jenv, jclass jcls, jstring jarg1, jlong jarg2, jlong jarg3) {
  jlong jresult = 0 ;
  char *arg1 = (char *) 0 ;
  pj_size_t arg2 ;
  pj_size_t arg3 ;
  pj_pool_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = 0;
  if (jarg1) {
    arg1 = (char *)jenv->GetStringUTFChars(jarg1, 0);
    if (!arg1) return 0;
  }
  arg2 = (pj_size_t)jarg2; 
  arg3 = (pj_size_t)jarg3; 
  result = (pj_pool_t *)pjsua_pool_create((char const *)arg1,arg2,arg3);
  *(pj_pool_t **)&jresult = result; 
  if (arg1) jenv->ReleaseStringUTFChars(jarg1, (const char *)arg1);
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_reconfigure_1logging(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_logging_config *arg1 = (pjsua_logging_config *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_logging_config **)&jarg1; 
  result = (pj_status_t)pjsua_reconfigure_logging((pjsua_logging_config const *)arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1pjsip_1endpt(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_endpoint *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_endpoint *)pjsua_get_pjsip_endpt();
  *(pjsip_endpoint **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1pjmedia_1endpt(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_endpt *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_endpt *)pjsua_get_pjmedia_endpt();
  *(pjmedia_endpt **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1pool_1factory(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pj_pool_factory *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pj_pool_factory *)pjsua_get_pool_factory();
  *(pj_pool_factory **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_detect_1nat_1type(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  result = (pj_status_t)pjsua_detect_nat_type();
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1nat_1type(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  jint jresult = 0 ;
  pj_stun_nat_type *arg1 = (pj_stun_nat_type *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pj_stun_nat_type **)&jarg1; 
  result = (pj_status_t)pjsua_get_nat_type(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_verify_1sip_1url(JNIEnv *jenv, jclass jcls, jstring jarg1) {
  jint jresult = 0 ;
  char *arg1 = (char *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = 0;
  if (jarg1) {
    arg1 = (char *)jenv->GetStringUTFChars(jarg1, 0);
    if (!arg1) return 0;
  }
  result = (pj_status_t)pjsua_verify_sip_url((char const *)arg1);
  jresult = (jint)result; 
  if (arg1) jenv->ReleaseStringUTFChars(jarg1, (const char *)arg1);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_perror(JNIEnv *jenv, jclass jcls, jstring jarg1, jstring jarg2, jint jarg3) {
  char *arg1 = (char *) 0 ;
  char *arg2 = (char *) 0 ;
  pj_status_t arg3 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = 0;
  if (jarg1) {
    arg1 = (char *)jenv->GetStringUTFChars(jarg1, 0);
    if (!arg1) return ;
  }
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  arg3 = (pj_status_t)jarg3; 
  pjsua_perror((char const *)arg1,(char const *)arg2,arg3);
  if (arg1) jenv->ReleaseStringUTFChars(jarg1, (const char *)arg1);
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_dump(JNIEnv *jenv, jclass jcls, jint jarg1) {
  pj_bool_t arg1 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pj_bool_t)jarg1; 
  pjsua_dump(arg1);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1port_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->port = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1port_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  result = (unsigned int) ((arg1)->port);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1public_1addr_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->public_addr = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1public_1addr_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->public_addr);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1bound_1addr_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->bound_addr = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1bound_1addr_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->bound_addr);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1tls_1setting_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pjsip_tls_setting arg2 ;
  pjsip_tls_setting *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  argp2 = *(pjsip_tls_setting **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjsip_tls_setting");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->tls_setting = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1tls_1setting_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pjsip_tls_setting result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  result =  ((arg1)->tls_setting);
  *(pjsip_tls_setting **)&jresult = new pjsip_tls_setting((const pjsip_tls_setting &)result); 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1transport_1config(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_transport_config *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_transport_config *)new pjsua_transport_config();
  *(pjsua_transport_config **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1transport_1config(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1config_1default(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  pjsua_transport_config_default(arg1);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1config_1dup(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pjsua_transport_config *arg2 = (pjsua_transport_config *) 0 ;
  pjsua_transport_config *arg3 = (pjsua_transport_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  (void)jarg3_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pjsua_transport_config **)&jarg2; 
  arg3 = *(pjsua_transport_config **)&jarg3; 
  pjsua_transport_config_dup(arg1,arg2,(pjsua_transport_config const *)arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pjsua_transport_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = (pjsua_transport_id)jarg2; 
  if (arg1) (arg1)->id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pjsua_transport_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (pjsua_transport_id) ((arg1)->id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pjsip_transport_type_e arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = (pjsip_transport_type_e)jarg2; 
  if (arg1) (arg1)->type = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pjsip_transport_type_e result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (pjsip_transport_type_e) ((arg1)->type);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->type_name = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->type_name);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->info = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->info);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1flag_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->flag = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1flag_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (unsigned int) ((arg1)->flag);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1addr_1len_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->addr_len = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1addr_1len_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (unsigned int) ((arg1)->addr_len);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1addr_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pj_sockaddr arg2 ;
  pj_sockaddr *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  argp2 = *(pj_sockaddr **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_sockaddr");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->local_addr = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1addr_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pj_sockaddr result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result =  ((arg1)->local_addr);
  *(pj_sockaddr **)&jresult = new pj_sockaddr((const pj_sockaddr &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pjsip_host_port arg2 ;
  pjsip_host_port *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  argp2 = *(pjsip_host_port **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjsip_host_port");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->local_name = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  pjsip_host_port result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result =  ((arg1)->local_name);
  *(pjsip_host_port **)&jresult = new pjsip_host_port((const pjsip_host_port &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1usage_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->usage_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1usage_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  result = (unsigned int) ((arg1)->usage_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1transport_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_transport_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_transport_info *)new pjsua_transport_info();
  *(pjsua_transport_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1transport_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_transport_info *arg1 = (pjsua_transport_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_transport_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1create(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jlong jarg3) {
  jint jresult = 0 ;
  pjsip_transport_type_e arg1 ;
  pjsua_transport_config *arg2 = (pjsua_transport_config *) 0 ;
  pjsua_transport_id *arg3 = (pjsua_transport_id *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsip_transport_type_e)jarg1; 
  arg2 = *(pjsua_transport_config **)&jarg2; 
  arg3 = *(pjsua_transport_id **)&jarg3; 
  result = (pj_status_t)pjsua_transport_create(arg1,(pjsua_transport_config const *)arg2,arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1register(JNIEnv *jenv, jclass jcls, jlong jarg1, jlong jarg2) {
  jint jresult = 0 ;
  pjsip_transport *arg1 = (pjsip_transport *) 0 ;
  pjsua_transport_id *arg2 = (pjsua_transport_id *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_transport **)&jarg1; 
  arg2 = *(pjsua_transport_id **)&jarg2; 
  result = (pj_status_t)pjsua_transport_register(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1get_1count(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  //TODO: reactivate
  //result = (unsigned int)pjsua_transport_get_count();
  result = 0;
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1transports(JNIEnv *jenv, jclass jcls, jintArray jarg1, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_transport_id *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  jint *jarr1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  if (!SWIG_JavaArrayInInt(jenv, &jarr1, &arg1, jarg1)) return 0; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_transports(arg1,arg2);
  jresult = (jint)result; 
  SWIG_JavaArrayArgoutInt(jenv, jarr1, arg1, jarg1); 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  delete [] arg1; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1get_1info(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_transport_id arg1 ;
  pjsua_transport_info *arg2 = (pjsua_transport_info *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_transport_id)jarg1; 
  arg2 = *(pjsua_transport_info **)&jarg2; 
  result = (pj_status_t)pjsua_transport_get_info(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1set_1enable(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_transport_id arg1 ;
  pj_bool_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_transport_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  result = (pj_status_t)pjsua_transport_set_enable(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_transport_1close(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_transport_id arg1 ;
  pj_bool_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_transport_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  result = (pj_status_t)pjsua_transport_close(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1ACC_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 8;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1REG_1INTERVAL_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 300;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1PUBLISH_1EXPIRATION_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 600;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1ACC_1PRIORITY_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 0;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1SECURE_1SCHEME_1get(JNIEnv *jenv, jclass jcls) {
  jstring jresult = 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (char *) "sips";
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1priority_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->priority = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1priority_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (int) ((arg1)->priority);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->id = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->id);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1uri_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->reg_uri = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1uri_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->reg_uri);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1publish_1enabled_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->publish_enabled = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1publish_1enabled_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->publish_enabled);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1auth_1pref_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pjsip_auth_clt_pref arg2 ;
  pjsip_auth_clt_pref *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  argp2 = *(pjsip_auth_clt_pref **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjsip_auth_clt_pref");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->auth_pref = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1auth_1pref_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pjsip_auth_clt_pref result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result =  ((arg1)->auth_pref);
  *(pjsip_auth_clt_pref **)&jresult = new pjsip_auth_clt_pref((const pjsip_auth_clt_pref &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1pidf_1tuple_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->pidf_tuple_id = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1pidf_1tuple_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->pidf_tuple_id);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1force_1contact_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->force_contact = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1force_1contact_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->force_contact);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1require_1100rel_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->require_100rel = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1require_1100rel_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->require_100rel);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1cnt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->proxy_cnt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1cnt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (unsigned int) ((arg1)->proxy_cnt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  {
    size_t ii;
    pj_str_t *b = (pj_str_t *) arg1->proxy;
    for (ii = 0; ii < (size_t)8; ii++) b[ii] = *((pj_str_t *) arg2 + ii);
  }
  
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_str_t *)(pj_str_t *) ((arg1)->proxy);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1timeout_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->reg_timeout = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1timeout_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (unsigned int) ((arg1)->reg_timeout);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->cred_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (unsigned int) ((arg1)->cred_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pjsip_cred_info *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pjsip_cred_info **)&jarg2; 
  {
    size_t ii;
    pjsip_cred_info *b = (pjsip_cred_info *) arg1->cred_info;
    for (ii = 0; ii < (size_t)8; ii++) b[ii] = *((pjsip_cred_info *) arg2 + ii);
  }
  
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pjsip_cred_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pjsip_cred_info *)(pjsip_cred_info *) ((arg1)->cred_info);
  *(pjsip_cred_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1transport_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pjsua_transport_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (pjsua_transport_id)jarg2; 
  if (arg1) (arg1)->transport_id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1transport_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pjsua_transport_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pjsua_transport_id) ((arg1)->transport_id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1allow_1contact_1rewrite_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->allow_contact_rewrite = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1allow_1contact_1rewrite_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->allow_contact_rewrite);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1interval_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->ka_interval = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1interval_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (unsigned int) ((arg1)->ka_interval);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1data_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->ka_data = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1data_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->ka_data);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1acc_1config(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_acc_config *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_acc_config *)new pjsua_acc_config();
  *(pjsua_acc_config **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1acc_1config(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1config_1default(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  pjsua_acc_config_default(arg1);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1config_1dup(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pjsua_acc_config *arg2 = (pjsua_acc_config *) 0 ;
  pjsua_acc_config *arg3 = (pjsua_acc_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  (void)jarg3_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pjsua_acc_config **)&jarg2; 
  arg3 = *(pjsua_acc_config **)&jarg3; 
  pjsua_acc_config_dup(arg1,arg2,(pjsua_acc_config const *)arg3);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pjsua_acc_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = (pjsua_acc_id)jarg2; 
  if (arg1) (arg1)->id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pjsua_acc_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pjsua_acc_id) ((arg1)->id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1is_1default_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->is_default = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1is_1default_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pj_bool_t) ((arg1)->is_default);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1acc_1uri_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->acc_uri = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1acc_1uri_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->acc_uri);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1has_1registration_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->has_registration = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1has_1registration_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pj_bool_t) ((arg1)->has_registration);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1expires_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->expires = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1expires_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (int) ((arg1)->expires);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pjsip_status_code arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = (pjsip_status_code)jarg2; 
  if (arg1) (arg1)->status = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pjsip_status_code) ((arg1)->status);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1text_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->status_text = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1text_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->status_text);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->online_status = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pj_bool_t) ((arg1)->online_status);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1text_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->online_status_text = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1text_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->online_status_text);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1rpid_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pjrpid_element arg2 ;
  pjrpid_element *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  argp2 = *(pjrpid_element **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjrpid_element");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->rpid = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1rpid_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  pjrpid_element result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result =  ((arg1)->rpid);
  *(pjrpid_element **)&jresult = new pjrpid_element((const pjrpid_element &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1buf_1_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->buf_, (const char *)arg2, PJ_ERR_MSG_SIZE);
    else arg1->buf_[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1buf_1_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  result = (char *)(char *) ((arg1)->buf_);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1acc_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_acc_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_acc_info *)new pjsua_acc_info();
  *(pjsua_acc_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1acc_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_acc_info *arg1 = (pjsua_acc_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1get_1count(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  result = (unsigned int)pjsua_acc_get_count();
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1is_1valid(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  result = (pj_bool_t)pjsua_acc_is_valid(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1default(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  result = (pj_status_t)pjsua_acc_set_default(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1get_1default(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pjsua_acc_id result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_acc_id)pjsua_acc_get_default();
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1add(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2, jintArray jarg3) {
  jint jresult = 0 ;
  pjsua_acc_config *arg1 = (pjsua_acc_config *) 0 ;
  pj_bool_t arg2 ;
  pjsua_acc_id *arg3 = (pjsua_acc_id *) 0 ;
  pjsua_acc_id temp3 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  {
    if (!jarg3) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg3) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg3 = &temp3; 
  }
  result = (pj_status_t)pjsua_acc_add((pjsua_acc_config const *)arg1,arg2,arg3);
  jresult = (jint)result; 

  {
    jint jvalue = (jint)temp3;
    jenv->SetIntArrayRegion(jarg3, 0, 1, &jvalue);
  }
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1add_1local(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2, jintArray jarg3) {
  jint jresult = 0 ;
  pjsua_transport_id arg1 ;
  pj_bool_t arg2 ;
  pjsua_acc_id *arg3 = (pjsua_acc_id *) 0 ;
  pjsua_acc_id temp3 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_transport_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  {
    if (!jarg3) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg3) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg3 = &temp3; 
  }
  result = (pj_status_t)pjsua_acc_add_local(arg1,arg2,arg3);
  jresult = (jint)result; 
  {
    jint jvalue = (jint)temp3;
    jenv->SetIntArrayRegion(jarg3, 0, 1, &jvalue);
  }
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1del(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  result = (pj_status_t)pjsua_acc_del(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1modify(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pjsua_acc_config *arg2 = (pjsua_acc_config *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = *(pjsua_acc_config **)&jarg2; 
  result = (pj_status_t)pjsua_acc_modify(arg1,(pjsua_acc_config const *)arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1online_1status(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_bool_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  result = (pj_status_t)pjsua_acc_set_online_status(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1online_1status2(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2, jlong jarg3) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_bool_t arg2 ;
  pjrpid_element *arg3 = (pjrpid_element *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  arg3 = *(pjrpid_element **)&jarg3; 
  result = (pj_status_t)pjsua_acc_set_online_status2(arg1,arg2,(pjrpid_element const *)arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1registration(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_bool_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  result = (pj_status_t)pjsua_acc_set_registration(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1get_1info(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pjsua_acc_info *arg2 = (pjsua_acc_info *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = *(pjsua_acc_info **)&jarg2; 
  result = (pj_status_t)pjsua_acc_get_info(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1accs(JNIEnv *jenv, jclass jcls, jintArray jarg1, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_acc_id *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  jint *jarr1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  if (!SWIG_JavaArrayInInt(jenv, &jarr1, &arg1, jarg1)) return 0; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_accs(arg1,arg2);
  jresult = (jint)result; 
  SWIG_JavaArrayArgoutInt(jenv, jarr1, arg1, jarg1); 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  delete [] arg1; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1enum_1info(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_acc_info *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_acc_info **)&jarg1; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_acc_enum_info(arg1,arg2);
  jresult = (jint)result; 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1find_1for_1outgoing(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  pjsua_acc_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  result = (pjsua_acc_id)pjsua_acc_find_for_outgoing((pj_str_t const *)arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1find_1for_1incoming(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  jint jresult = 0 ;
  pjsip_rx_data *arg1 = (pjsip_rx_data *) 0 ;
  pjsua_acc_id result;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsip_rx_data **)&jarg1; 
  result = (pjsua_acc_id)pjsua_acc_find_for_incoming(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1create_1request(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jlong jarg3, jobject jarg3_, jlong jarg4) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pjsip_method *arg2 = (pjsip_method *) 0 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pjsip_tx_data **arg4 = (pjsip_tx_data **) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg3_;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = *(pjsip_method **)&jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pjsip_tx_data ***)&jarg4; 
  result = (pj_status_t)pjsua_acc_create_request(arg1,(pjsip_method const *)arg2,(pj_str_t const *)arg3,arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1create_1uac_1contact(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jint jarg3, jlong jarg4, jobject jarg4_) {
  jint jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pjsua_acc_id arg3 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  (void)jarg4_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = (pjsua_acc_id)jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  result = (pj_status_t)pjsua_acc_create_uac_contact(arg1,arg2,arg3,(pj_str_t const *)arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1create_1uas_1contact(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jint jarg3, jlong jarg4) {
  jint jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pjsua_acc_id arg3 ;
  pjsip_rx_data *arg4 = (pjsip_rx_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = (pjsua_acc_id)jarg3; 
  arg4 = *(pjsip_rx_data **)&jarg4; 
  result = (pj_status_t)pjsua_acc_create_uas_contact(arg1,arg2,arg3,arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1transport(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pjsua_transport_id arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = (pjsua_transport_id)jarg2; 
  result = (pj_status_t)pjsua_acc_set_transport(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1CALLS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 32;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_call_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  if (arg1) (arg1)->id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_call_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsua_call_id) ((arg1)->id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1role_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsip_role_e arg2 ;
  pjsip_role_e *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  argp2 = *(pjsip_role_e **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjsip_role_e");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->role = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1role_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsip_role_e result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result =  ((arg1)->role);
  *(pjsip_role_e **)&jresult = new pjsip_role_e((const pjsip_role_e &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1acc_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_acc_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjsua_acc_id)jarg2; 
  if (arg1) (arg1)->acc_id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1acc_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_acc_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsua_acc_id) ((arg1)->acc_id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->local_info = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->local_info);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1contact_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->local_contact = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1contact_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->local_contact);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->remote_info = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->remote_info);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1contact_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->remote_contact = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1contact_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->remote_contact);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1call_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->call_id = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1call_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->call_id);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsip_inv_state arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjsip_inv_state)jarg2; 
  if (arg1) (arg1)->state = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsip_inv_state result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsip_inv_state) ((arg1)->state);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1text_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->state_text = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1text_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->state_text);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsip_status_code arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjsip_status_code)jarg2; 
  if (arg1) (arg1)->last_status = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsip_status_code result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsip_status_code) ((arg1)->last_status);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1text_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->last_status_text = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1text_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->last_status_text);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_call_media_status arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjsua_call_media_status)jarg2; 
  if (arg1) (arg1)->media_status = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_call_media_status result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsua_call_media_status) ((arg1)->media_status);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1dir_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjmedia_dir arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjmedia_dir)jarg2; 
  if (arg1) (arg1)->media_dir = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1dir_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjmedia_dir result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjmedia_dir) ((arg1)->media_dir);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1conf_1slot_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_conf_port_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  arg2 = (pjsua_conf_port_id)jarg2; 
  if (arg1) (arg1)->conf_slot = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1conf_1slot_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_conf_port_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsua_conf_port_id) ((arg1)->conf_slot);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1connect_1duration_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_time_val arg2 ;
  pj_time_val *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  argp2 = *(pj_time_val **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_time_val");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->connect_duration = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1connect_1duration_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_time_val result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result =  ((arg1)->connect_duration);
  *(pj_time_val **)&jresult = new pj_time_val((const pj_time_val &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1total_1duration_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_time_val arg2 ;
  pj_time_val *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  argp2 = *(pj_time_val **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_time_val");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->total_duration = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1total_1duration_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pj_time_val result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result =  ((arg1)->total_duration);
  *(pj_time_val **)&jresult = new pj_time_val((const pj_time_val &)result); 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  pjsua_call_info_buf_ *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info **)&jarg1; 
  result = (pjsua_call_info_buf_ *)& ((arg1)->buf_);
  *(pjsua_call_info_buf_ **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1call_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_call_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_call_info *)new pjsua_call_info();
  *(pjsua_call_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1call_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_call_info *arg1 = (pjsua_call_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_call_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->local_info, (const char *)arg2, 128);
    else arg1->local_info[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  result = (char *)(char *) ((arg1)->local_info);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1contact_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->local_contact, (const char *)arg2, 128);
    else arg1->local_contact[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1contact_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  result = (char *)(char *) ((arg1)->local_contact);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1info_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->remote_info, (const char *)arg2, 128);
    else arg1->remote_info[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1info_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  result = (char *)(char *) ((arg1)->remote_info);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1contact_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->remote_contact, (const char *)arg2, 128);
    else arg1->remote_contact[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1contact_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  result = (char *)(char *) ((arg1)->remote_contact);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1call_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->call_id, (const char *)arg2, 128);
    else arg1->call_id[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1call_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  result = (char *)(char *) ((arg1)->call_id);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1last_1status_1text_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->last_status_text, (const char *)arg2, 128);
    else arg1->last_status_text[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1last_1status_1text_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  result = (char *)(char *) ((arg1)->last_status_text);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1call_1info_1buf_1(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_call_info_buf_ *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_call_info_buf_ *)new pjsua_call_info_buf_();
  *(pjsua_call_info_buf_ **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1call_1info_1buf_1(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_call_info_buf_ *arg1 = (pjsua_call_info_buf_ *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_call_info_buf_ **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1max_1count(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  result = (unsigned int)pjsua_call_get_max_count();
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1count(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  result = (unsigned int)pjsua_call_get_count();
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1calls(JNIEnv *jenv, jclass jcls, jintArray jarg1, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_call_id *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  jint *jarr1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  if (!SWIG_JavaArrayInInt(jenv, &jarr1, &arg1, jarg1)) return 0; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_calls(arg1,arg2);
  jresult = (jint)result; 
  SWIG_JavaArrayArgoutInt(jenv, jarr1, arg1, jarg1); 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  delete [] arg1; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1make_1call(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jlong jarg3, void * jarg4, jlong jarg5, jobject jarg5_, jintArray jarg6) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  unsigned int arg3 ;
  void *arg4 = (void *) 0 ;
  pjsua_msg_data *arg5 = (pjsua_msg_data *) 0 ;
  pjsua_call_id *arg6 = (pjsua_call_id *) 0 ;
  pjsua_call_id temp6 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  (void)jarg5_;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = (unsigned int)jarg3; 
  
  arg4 = jarg4;
  
  arg5 = *(pjsua_msg_data **)&jarg5; 
  {
    if (!jarg6) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg6) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg6 = &temp6; 
  }
  result = (pj_status_t)pjsua_call_make_call(arg1,(pj_str_t const *)arg2,arg3,arg4,(pjsua_msg_data const *)arg5,arg6);
  jresult = (jint)result; 
  {
    jint jvalue = (jint)temp6;
    jenv->SetIntArrayRegion(jarg6, 0, 1, &jvalue);
  }
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1is_1active(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_call_id)jarg1; 
  result = (pj_bool_t)pjsua_call_is_active(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1has_1media(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_call_id)jarg1; 
  result = (pj_bool_t)pjsua_call_has_media(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1conf_1port(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pjsua_conf_port_id result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_call_id)jarg1; 
  result = (pjsua_conf_port_id)pjsua_call_get_conf_port(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1info(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pjsua_call_info *arg2 = (pjsua_call_info *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pjsua_call_info **)&jarg2; 
  result = (pj_status_t)pjsua_call_get_info(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1set_1user_1data(JNIEnv *jenv, jclass jcls, jint jarg1, void * jarg2) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  void *arg2 = (void *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_call_id)jarg1; 
  
  arg2 = jarg2;
  
  result = (pj_status_t)pjsua_call_set_user_data(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void * JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1user_1data(JNIEnv *jenv, jclass jcls, jint jarg1) {
  void * jresult = 0 ;
  pjsua_call_id arg1 ;
  void *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_call_id)jarg1; 
  result = (void *)pjsua_call_get_user_data(arg1);
  
  jresult = result; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1rem_1nat_1type(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_stun_nat_type *arg2 = (pj_stun_nat_type *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pj_stun_nat_type **)&jarg2; 
  result = (pj_status_t)pjsua_call_get_rem_nat_type(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1answer(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  unsigned int arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pjsua_msg_data *arg4 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg3_;
  (void)jarg4_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (unsigned int)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pjsua_msg_data **)&jarg4; 
  result = (pj_status_t)pjsua_call_answer(arg1,arg2,(pj_str_t const *)arg3,(pjsua_msg_data const *)arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1hangup(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  unsigned int arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pjsua_msg_data *arg4 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg3_;
  (void)jarg4_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (unsigned int)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pjsua_msg_data **)&jarg4; 
  result = (pj_status_t)pjsua_call_hangup(arg1,arg2,(pj_str_t const *)arg3,(pjsua_msg_data const *)arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1set_1hold(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pjsua_msg_data *arg2 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pjsua_msg_data **)&jarg2; 
  result = (pj_status_t)pjsua_call_set_hold(arg1,(pjsua_msg_data const *)arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1reinvite(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2, jlong jarg3, jobject jarg3_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_bool_t arg2 ;
  pjsua_msg_data *arg3 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg3_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  arg3 = *(pjsua_msg_data **)&jarg3; 
  result = (pj_status_t)pjsua_call_reinvite(arg1,arg2,(pjsua_msg_data const *)arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1update(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jlong jarg3, jobject jarg3_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  unsigned int arg2 ;
  pjsua_msg_data *arg3 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg3_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (unsigned int)jarg2; 
  arg3 = *(pjsua_msg_data **)&jarg3; 
  result = (pj_status_t)pjsua_call_update(arg1,arg2,(pjsua_msg_data const *)arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1xfer(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pjsua_msg_data *arg3 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  (void)jarg3_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = *(pjsua_msg_data **)&jarg3; 
  result = (pj_status_t)pjsua_call_xfer(arg1,(pj_str_t const *)arg2,(pjsua_msg_data const *)arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1XFER_1NO_1REQUIRE_1REPLACES_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 1;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1xfer_1replaces(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2, jlong jarg3, jlong jarg4, jobject jarg4_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pjsua_call_id arg2 ;
  unsigned int arg3 ;
  pjsua_msg_data *arg4 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg4_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (pjsua_call_id)jarg2; 
  arg3 = (unsigned int)jarg3; 
  arg4 = *(pjsua_msg_data **)&jarg4; 
  result = (pj_status_t)pjsua_call_xfer_replaces(arg1,arg2,arg3,(pjsua_msg_data const *)arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1dial_1dtmf(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  result = (pj_status_t)pjsua_call_dial_dtmf(arg1,(pj_str_t const *)arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1send_1im(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, void * jarg5) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pjsua_msg_data *arg4 = (pjsua_msg_data *) 0 ;
  void *arg5 = (void *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  (void)jarg3_;
  (void)jarg4_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pjsua_msg_data **)&jarg4; 
  
  arg5 = jarg5;
  
  result = (pj_status_t)pjsua_call_send_im(arg1,(pj_str_t const *)arg2,(pj_str_t const *)arg3,(pjsua_msg_data const *)arg4,arg5);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1send_1typing_1ind(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2, jlong jarg3, jobject jarg3_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_bool_t arg2 ;
  pjsua_msg_data *arg3 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg3_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  arg3 = *(pjsua_msg_data **)&jarg3; 
  result = (pj_status_t)pjsua_call_send_typing_ind(arg1,arg2,(pjsua_msg_data const *)arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1send_1request(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_) {
  jint jresult = 0 ;
  pjsua_call_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pjsua_msg_data *arg3 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  (void)jarg3_;
  arg1 = (pjsua_call_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = *(pjsua_msg_data **)&jarg3; 
  result = (pj_status_t)pjsua_call_send_request(arg1,(pj_str_t const *)arg2,(pjsua_msg_data const *)arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1hangup_1all(JNIEnv *jenv, jclass jcls) {
  (void)jenv;
  (void)jcls;
  pjsua_call_hangup_all();
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_call_1dump(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2, jstring jarg5) {
  jstring jresult ;
  pjsua_call_id arg1 ;
  pj_bool_t arg2 ;
  char *arg5 = (char *) 0 ;
  
  (void)jenv;
  (void)jcls;

  char some_buf[1024 * 4];

  arg1 = (pjsua_call_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 

  arg5 = 0;
  if (jarg5) {
    arg5 = (char *)jenv->GetStringUTFChars(jarg5, 0);
    if (!arg5) return 0;
  }

  (pj_status_t)pjsua_call_dump(arg1,arg2,some_buf,sizeof(some_buf),(char const *)arg5);

  if (arg5) jenv->ReleaseStringUTFChars(jarg5, (const char *)arg5);


  if(some_buf) jresult = jenv->NewStringUTF((const char *)some_buf);
  return jresult;
}




SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1BUDDIES_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 256;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1PRES_1TIMER_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 300;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1uri_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->uri = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1uri_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->uri);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1subscribe_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->subscribe = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1subscribe_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->subscribe);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1buddy_1config(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_buddy_config *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_buddy_config *)new pjsua_buddy_config();
  *(pjsua_buddy_config **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1buddy_1config(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pjsua_buddy_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = (pjsua_buddy_id)jarg2; 
  if (arg1) (arg1)->id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pjsua_buddy_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (pjsua_buddy_id) ((arg1)->id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1uri_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->uri = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1uri_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->uri);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1contact_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->contact = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1contact_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->contact);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pjsua_buddy_status arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = (pjsua_buddy_status)jarg2; 
  if (arg1) (arg1)->status = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pjsua_buddy_status result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (pjsua_buddy_status) ((arg1)->status);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1text_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->status_text = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1text_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->status_text);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1monitor_1pres_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->monitor_pres = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1monitor_1pres_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (pj_bool_t) ((arg1)->monitor_pres);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1rpid_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pjrpid_element arg2 ;
  pjrpid_element *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  argp2 = *(pjrpid_element **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjrpid_element");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->rpid = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1rpid_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  pjrpid_element result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result =  ((arg1)->rpid);
  *(pjrpid_element **)&jresult = new pjrpid_element((const pjrpid_element &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1buf_1_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->buf_, (const char *)arg2, 512);
    else arg1->buf_[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1buf_1_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  result = (char *)(char *) ((arg1)->buf_);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1buddy_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_buddy_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_buddy_info *)new pjsua_buddy_info();
  *(pjsua_buddy_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1buddy_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_buddy_info *arg1 = (pjsua_buddy_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_buddy_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1config_1default(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  pjsua_buddy_config_default(arg1);
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1buddy_1count(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  result = (unsigned int)pjsua_get_buddy_count();
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1is_1valid(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_buddy_id arg1 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_buddy_id)jarg1; 
  result = (pj_bool_t)pjsua_buddy_is_valid(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1buddies(JNIEnv *jenv, jclass jcls, jintArray jarg1, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_buddy_id *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  jint *jarr1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  if (!SWIG_JavaArrayInInt(jenv, &jarr1, &arg1, jarg1)) return 0; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_buddies(arg1,arg2);
  jresult = (jint)result; 
  SWIG_JavaArrayArgoutInt(jenv, jarr1, arg1, jarg1); 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  delete [] arg1; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1get_1info(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_buddy_id arg1 ;
  pjsua_buddy_info *arg2 = (pjsua_buddy_info *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_buddy_id)jarg1; 
  arg2 = *(pjsua_buddy_info **)&jarg2; 
  result = (pj_status_t)pjsua_buddy_get_info(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1add(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  jint jresult = 0 ;
  pjsua_buddy_config *arg1 = (pjsua_buddy_config *) 0 ;
  pjsua_buddy_id *arg2 = (pjsua_buddy_id *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_buddy_config **)&jarg1; 
  arg2 = *(pjsua_buddy_id **)&jarg2; 
  result = (pj_status_t)pjsua_buddy_add((pjsua_buddy_config const *)arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1del(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_buddy_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_buddy_id)jarg1; 
  result = (pj_status_t)pjsua_buddy_del(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1subscribe_1pres(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_buddy_id arg1 ;
  pj_bool_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_buddy_id)jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  result = (pj_status_t)pjsua_buddy_subscribe_pres(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_buddy_1update_1pres(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_buddy_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_buddy_id)jarg1; 
  result = (pj_status_t)pjsua_buddy_update_pres(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pres_1dump(JNIEnv *jenv, jclass jcls, jint jarg1) {
  pj_bool_t arg1 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pj_bool_t)jarg1; 
  pjsua_pres_dump(arg1);
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1message_1method_1get(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsip_method result;
  
  (void)jenv;
  (void)jcls;
  result = (pjsip_method)pjsip_message_method;
  *(pjsip_method **)&jresult = new pjsip_method((const pjsip_method &)result); 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_im_1send(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jlong jarg3, jobject jarg3_, jlong jarg4, jobject jarg4_, jlong jarg5, jobject jarg5_, void * jarg6) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  pj_str_t *arg4 = (pj_str_t *) 0 ;
  pjsua_msg_data *arg5 = (pjsua_msg_data *) 0 ;
  void *arg6 = (void *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  (void)jarg3_;
  (void)jarg4_;
  (void)jarg5_;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = *(pj_str_t **)&jarg4; 
  arg5 = *(pjsua_msg_data **)&jarg5; 
  
  arg6 = jarg6;
  
  result = (pj_status_t)pjsua_im_send(arg1,(pj_str_t const *)arg2,(pj_str_t const *)arg3,(pj_str_t const *)arg4,(pjsua_msg_data const *)arg5,arg6);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_im_1typing(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_, jint jarg3, jlong jarg4, jobject jarg4_) {
  jint jresult = 0 ;
  pjsua_acc_id arg1 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  pj_bool_t arg3 ;
  pjsua_msg_data *arg4 = (pjsua_msg_data *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  (void)jarg4_;
  arg1 = (pjsua_acc_id)jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  arg3 = (pj_bool_t)jarg3; 
  arg4 = *(pjsua_msg_data **)&jarg4; 
  result = (pj_status_t)pjsua_im_typing(arg1,(pj_str_t const *)arg2,arg3,(pjsua_msg_data const *)arg4);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1CONF_1PORTS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 254;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1CLOCK_1RATE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 16000;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1AUDIO_1FRAME_1PTIME_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 20;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1CODEC_1QUALITY_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 8;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1ILBC_1MODE_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 30;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1EC_1TAIL_1LEN_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 200;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1PLAYERS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 32;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1RECORDERS_1get(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  result = (int) 32;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1clock_1rate_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->clock_rate = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1clock_1rate_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->clock_rate);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1clock_1rate_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->snd_clock_rate = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1clock_1rate_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->snd_clock_rate);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1channel_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->channel_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1channel_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->channel_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1audio_1frame_1ptime_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->audio_frame_ptime = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1audio_1frame_1ptime_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->audio_frame_ptime);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1max_1media_1ports_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->max_media_ports = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1max_1media_1ports_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->max_media_ports);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1has_1ioqueue_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->has_ioqueue = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1has_1ioqueue_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->has_ioqueue);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1thread_1cnt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->thread_cnt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1thread_1cnt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->thread_cnt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1quality_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->quality = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1quality_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->quality);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ptime_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->ptime = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ptime_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->ptime);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1no_1vad_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->no_vad = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1no_1vad_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->no_vad);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ilbc_1mode_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->ilbc_mode = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ilbc_1mode_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->ilbc_mode);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1tx_1drop_1pct_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->tx_drop_pct = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1tx_1drop_1pct_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->tx_drop_pct);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1rx_1drop_1pct_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->rx_drop_pct = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1rx_1drop_1pct_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->rx_drop_pct);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1options_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->ec_options = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1options_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->ec_options);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1tail_1len_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->ec_tail_len = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1tail_1len_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (unsigned int) ((arg1)->ec_tail_len);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1init_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->jb_init = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1init_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (int) ((arg1)->jb_init);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1min_1pre_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->jb_min_pre = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1min_1pre_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (int) ((arg1)->jb_min_pre);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1pre_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->jb_max_pre = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1pre_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (int) ((arg1)->jb_max_pre);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->jb_max = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (int) ((arg1)->jb_max);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1ice_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->enable_ice = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1ice_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->enable_ice);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ice_1no_1host_1cands_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  //TODO: reactivate
  //arg1 = *(pjsua_media_config **)&jarg1;
  //arg2 = (pj_bool_t)jarg2;
  //if (arg1) (arg1)->ice_no_host_cands = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ice_1no_1host_1cands_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  //TODO: reactivate
  //arg1 = *(pjsua_media_config **)&jarg1;
  //result = (pj_bool_t) ((arg1)->ice_no_host_cands);
  result = 0;
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1turn_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (pj_bool_t)jarg2; 
  if (arg1) (arg1)->enable_turn = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1turn_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_bool_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (pj_bool_t) ((arg1)->enable_turn);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1server_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->turn_server = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1server_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->turn_server);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1conn_1type_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_turn_tp_type arg2 ;
  pj_turn_tp_type *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  argp2 = *(pj_turn_tp_type **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_turn_tp_type");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->turn_conn_type = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1conn_1type_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_turn_tp_type result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result =  ((arg1)->turn_conn_type);
  *(pj_turn_tp_type **)&jresult = new pj_turn_tp_type((const pj_turn_tp_type &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1auth_1cred_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_stun_auth_cred arg2 ;
  pj_stun_auth_cred *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  argp2 = *(pj_stun_auth_cred **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_stun_auth_cred");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->turn_auth_cred = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1auth_1cred_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  pj_stun_auth_cred result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result =  ((arg1)->turn_auth_cred);
  *(pj_stun_auth_cred **)&jresult = new pj_stun_auth_cred((const pj_stun_auth_cred &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1auto_1close_1time_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  arg2 = (int)jarg2; 
  if (arg1) (arg1)->snd_auto_close_time = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1auto_1close_1time_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  result = (int) ((arg1)->snd_auto_close_time);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1media_1config(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_media_config *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_media_config *)new pjsua_media_config();
  *(pjsua_media_config **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1media_1config(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_media_config **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_media_1config_1default(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  pjsua_media_config *arg1 = (pjsua_media_config *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_config **)&jarg1; 
  pjsua_media_config_default(arg1);
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1codec_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->codec_id = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1codec_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->codec_id);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1priority_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  pj_uint8_t arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  if (arg1) (arg1)->priority = arg2;
}


SWIGEXPORT jshort JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1priority_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jshort jresult = 0 ;
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  pj_uint8_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  result = (pj_uint8_t) ((arg1)->priority);
  jresult = (jshort)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1buf_1_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jstring jarg2) {
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  char *arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  arg2 = 0;
  if (jarg2) {
    arg2 = (char *)jenv->GetStringUTFChars(jarg2, 0);
    if (!arg2) return ;
  }
  {
    if (arg2) strncpy((char *)arg1->buf_, (const char *)arg2, 32);
    else arg1->buf_[0] = 0;
  }
  
  if (arg2) jenv->ReleaseStringUTFChars(jarg2, (const char *)arg2);
}


SWIGEXPORT jstring JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1buf_1_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jstring jresult = 0 ;
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  char *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  result = (char *)(char *) ((arg1)->buf_);
  if(result) jresult = jenv->NewStringUTF((const char *)result);
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1codec_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_codec_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_codec_info *)new pjsua_codec_info();
  *(pjsua_codec_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1codec_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_codec_info *arg1 = (pjsua_codec_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1slot_1id_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jint jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  pjsua_conf_port_id arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = (pjsua_conf_port_id)jarg2; 
  if (arg1) (arg1)->slot_id = arg2;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1slot_1id_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  pjsua_conf_port_id result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (pjsua_conf_port_id) ((arg1)->slot_id);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1name_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  pj_str_t *arg2 = (pj_str_t *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = *(pj_str_t **)&jarg2; 
  if (arg1) (arg1)->name = *arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1name_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  pj_str_t *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (pj_str_t *)& ((arg1)->name);
  *(pj_str_t **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1clock_1rate_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->clock_rate = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1clock_1rate_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->clock_rate);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1channel_1count_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->channel_count = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1channel_1count_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->channel_count);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1samples_1per_1frame_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->samples_per_frame = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1samples_1per_1frame_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->samples_per_frame);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1bits_1per_1sample_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->bits_per_sample = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1bits_1per_1sample_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->bits_per_sample);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listener_1cnt_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int arg2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  if (arg1) (arg1)->listener_cnt = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listener_1cnt_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (unsigned int) ((arg1)->listener_cnt);
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listeners_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jintArray jarg2) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  pjsua_conf_port_id *arg2 ;
  jint *jarr2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  if (jarg2 && jenv->GetArrayLength(jarg2) != 254) {
    SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "incorrect array size");
    return ;
  }
  if (!SWIG_JavaArrayInInt(jenv, &jarr2, &arg2, jarg2)) return ; 
  {
    size_t ii;
    pjsua_conf_port_id *b = (pjsua_conf_port_id *) arg1->listeners;
    for (ii = 0; ii < (size_t)254; ii++) b[ii] = *((pjsua_conf_port_id *) arg2 + ii);
  }
  SWIG_JavaArrayArgoutInt(jenv, jarr2, arg2, jarg2); 
  delete [] arg2; 
}


SWIGEXPORT jintArray JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listeners_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jintArray jresult = 0 ;
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  pjsua_conf_port_id *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  result = (pjsua_conf_port_id *)(pjsua_conf_port_id *) ((arg1)->listeners);
  jresult = SWIG_JavaArrayOutInt(jenv, result, 254); 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1conf_1port_1info(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_conf_port_info *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_conf_port_info *)new pjsua_conf_port_info();
  *(pjsua_conf_port_info **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1conf_1port_1info(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_conf_port_info *arg1 = (pjsua_conf_port_info *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_conf_port_info **)&jarg1; 
  delete arg1;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1skinfo_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_transport *arg1 = (pjsua_media_transport *) 0 ;
  pjmedia_sock_info arg2 ;
  pjmedia_sock_info *argp2 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_transport **)&jarg1; 
  argp2 = *(pjmedia_sock_info **)&jarg2; 
  if (!argp2) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pjmedia_sock_info");
    return ;
  }
  arg2 = *argp2; 
  if (arg1) (arg1)->skinfo = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1skinfo_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_transport *arg1 = (pjsua_media_transport *) 0 ;
  pjmedia_sock_info result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_transport **)&jarg1; 
  result =  ((arg1)->skinfo);
  *(pjmedia_sock_info **)&jresult = new pjmedia_sock_info((const pjmedia_sock_info &)result); 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1transport_1set(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2) {
  pjsua_media_transport *arg1 = (pjsua_media_transport *) 0 ;
  pjmedia_transport *arg2 = (pjmedia_transport *) 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_transport **)&jarg1; 
  arg2 = *(pjmedia_transport **)&jarg2; 
  if (arg1) (arg1)->transport = arg2;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1transport_1get(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jlong jresult = 0 ;
  pjsua_media_transport *arg1 = (pjsua_media_transport *) 0 ;
  pjmedia_transport *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_media_transport **)&jarg1; 
  result = (pjmedia_transport *) ((arg1)->transport);
  *(pjmedia_transport **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1media_1transport(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjsua_media_transport *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjsua_media_transport *)new pjsua_media_transport();
  *(pjsua_media_transport **)&jresult = result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1media_1transport(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  pjsua_media_transport *arg1 = (pjsua_media_transport *) 0 ;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(pjsua_media_transport **)&jarg1; 
  delete arg1;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1max_1ports(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  result = (unsigned int)pjsua_conf_get_max_ports();
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1active_1ports(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  unsigned int result;
  
  (void)jenv;
  (void)jcls;
  result = (unsigned int)pjsua_conf_get_active_ports();
  jresult = (jlong)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1conf_1ports(JNIEnv *jenv, jclass jcls, jintArray jarg1, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_conf_port_id *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  jint *jarr1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  if (!SWIG_JavaArrayInInt(jenv, &jarr1, &arg1, jarg1)) return 0; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_conf_ports(arg1,arg2);
  jresult = (jint)result; 
  SWIG_JavaArrayArgoutInt(jenv, jarr1, arg1, jarg1); 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  delete [] arg1; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1port_1info(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  pjsua_conf_port_info *arg2 = (pjsua_conf_port_info *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg2_;
  arg1 = (pjsua_conf_port_id)jarg1; 
  arg2 = *(pjsua_conf_port_info **)&jarg2; 
  result = (pj_status_t)pjsua_conf_get_port_info(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1add_1port(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_, jintArray jarg3) {
  jint jresult = 0 ;
  pj_pool_t *arg1 = (pj_pool_t *) 0 ;
  pjmedia_port *arg2 = (pjmedia_port *) 0 ;
  pjsua_conf_port_id *arg3 = (pjsua_conf_port_id *) 0 ;
  pjsua_conf_port_id temp3 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pj_pool_t **)&jarg1; 
  arg2 = *(pjmedia_port **)&jarg2; 
  {
    if (!jarg3) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg3) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg3 = &temp3; 
  }
  result = (pj_status_t)pjsua_conf_add_port(arg1,arg2,arg3);
  jresult = (jint)result; 
  {
    jint jvalue = (jint)temp3;
    jenv->SetIntArrayRegion(jarg3, 0, 1, &jvalue);
  }
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1remove_1port(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_conf_port_id)jarg1; 
  result = (pj_status_t)pjsua_conf_remove_port(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1connect(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  pjsua_conf_port_id arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_conf_port_id)jarg1; 
  arg2 = (pjsua_conf_port_id)jarg2; 
  result = (pj_status_t)pjsua_conf_connect(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1disconnect(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  pjsua_conf_port_id arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_conf_port_id)jarg1; 
  arg2 = (pjsua_conf_port_id)jarg2; 
  result = (pj_status_t)pjsua_conf_disconnect(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1adjust_1tx_1level(JNIEnv *jenv, jclass jcls, jint jarg1, jfloat jarg2) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  float arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_conf_port_id)jarg1; 
  arg2 = (float)jarg2; 
  result = (pj_status_t)pjsua_conf_adjust_tx_level(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1adjust_1rx_1level(JNIEnv *jenv, jclass jcls, jint jarg1, jfloat jarg2) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  float arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_conf_port_id)jarg1; 
  arg2 = (float)jarg2; 
  result = (pj_status_t)pjsua_conf_adjust_rx_level(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1signal_1level(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2, jlong jarg3) {
  jint jresult = 0 ;
  pjsua_conf_port_id arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  unsigned int *arg3 = (unsigned int *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_conf_port_id)jarg1; 
  arg2 = *(unsigned int **)&jarg2; 
  arg3 = *(unsigned int **)&jarg3; 
  result = (pj_status_t)pjsua_conf_get_signal_level(arg1,arg2,arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_player_1create(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jlong jarg3) {
  jint jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  unsigned int arg2 ;
  pjsua_player_id *arg3 = (pjsua_player_id *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  arg3 = *(pjsua_player_id **)&jarg3; 
  result = (pj_status_t)pjsua_player_create((pj_str_t const *)arg1,arg2,arg3);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_playlist_1create(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jlong jarg3, jobject jarg3_, jlong jarg4, jlong jarg5) {
  jint jresult = 0 ;
  pj_str_t *arg1 ;
  unsigned int arg2 ;
  pj_str_t *arg3 = (pj_str_t *) 0 ;
  unsigned int arg4 ;
  pjsua_player_id *arg5 = (pjsua_player_id *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg3_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  arg3 = *(pj_str_t **)&jarg3; 
  arg4 = (unsigned int)jarg4; 
  arg5 = *(pjsua_player_id **)&jarg5; 
  result = (pj_status_t)pjsua_playlist_create((pj_str_t const (*))arg1,arg2,(pj_str_t const *)arg3,arg4,arg5);
  jresult = (jint)result; 
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_player_1get_1conf_1port(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_player_id arg1 ;
  pjsua_conf_port_id result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_player_id)jarg1; 
  result = (pjsua_conf_port_id)pjsua_player_get_conf_port(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_player_1get_1port(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2) {
  jint jresult = 0 ;
  pjsua_player_id arg1 ;
  pjmedia_port **arg2 = (pjmedia_port **) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_player_id)jarg1; 
  arg2 = *(pjmedia_port ***)&jarg2; 
  result = (pj_status_t)pjsua_player_get_port(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_player_1set_1pos(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2) {
  jint jresult = 0 ;
  pjsua_player_id arg1 ;
  pj_uint32_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_player_id)jarg1; 
  arg2 = (pj_uint32_t)jarg2; 
  result = (pj_status_t)pjsua_player_set_pos(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_player_1destroy(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_player_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_player_id)jarg1; 
  result = (pj_status_t)pjsua_player_destroy(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_recorder_1create(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, void * jarg3, jlong jarg4, jlong jarg5, jlong jarg6) {
  jint jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  unsigned int arg2 ;
  void *arg3 = (void *) 0 ;
  pj_ssize_t arg4 ;
  unsigned int arg5 ;
  pjsua_recorder_id *arg6 = (pjsua_recorder_id *) 0 ;
  pj_ssize_t *argp4 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = (unsigned int)jarg2; 
  
  arg3 = jarg3;
  
  argp4 = *(pj_ssize_t **)&jarg4; 
  if (!argp4) {
    SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "Attempt to dereference null pj_ssize_t");
    return 0;
  }
  arg4 = *argp4; 
  arg5 = (unsigned int)jarg5; 
  arg6 = *(pjsua_recorder_id **)&jarg6; 
  result = (pj_status_t)pjsua_recorder_create((pj_str_t const *)arg1,arg2,arg3,arg4,arg5,arg6);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_recorder_1get_1conf_1port(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_recorder_id arg1 ;
  pjsua_conf_port_id result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_recorder_id)jarg1; 
  result = (pjsua_conf_port_id)pjsua_recorder_get_conf_port(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_recorder_1get_1port(JNIEnv *jenv, jclass jcls, jint jarg1, jlong jarg2) {
  jint jresult = 0 ;
  pjsua_recorder_id arg1 ;
  pjmedia_port **arg2 = (pjmedia_port **) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_recorder_id)jarg1; 
  arg2 = *(pjmedia_port ***)&jarg2; 
  result = (pj_status_t)pjsua_recorder_get_port(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_recorder_1destroy(JNIEnv *jenv, jclass jcls, jint jarg1) {
  jint jresult = 0 ;
  pjsua_recorder_id arg1 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (pjsua_recorder_id)jarg1; 
  result = (pj_status_t)pjsua_recorder_destroy(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1snd_1devs(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlongArray jarg2) {
  jint jresult = 0 ;
  pjmedia_snd_dev_info *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjmedia_snd_dev_info **)&jarg1; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_snd_devs(arg1,arg2);
  jresult = (jint)result; 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1snd_1dev(JNIEnv *jenv, jclass jcls, jintArray jarg1, jintArray jarg2) {
  jint jresult = 0 ;
  int *arg1 = (int *) 0 ;
  int *arg2 = (int *) 0 ;
  int temp1 ;
  int temp2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  {
    if (!jarg1) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg1) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg1 = &temp1; 
  }
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = &temp2; 
  }
  result = (pj_status_t)pjsua_get_snd_dev(arg1,arg2);
  jresult = (jint)result; 
  {
    jint jvalue = (jint)temp1;
    jenv->SetIntArrayRegion(jarg1, 0, 1, &jvalue);
  }
  {
    jint jvalue = (jint)temp2;
    jenv->SetIntArrayRegion(jarg2, 0, 1, &jvalue);
  }
  
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_set_1snd_1dev(JNIEnv *jenv, jclass jcls, jint jarg1, jint jarg2) {
  jint jresult = 0 ;
  int arg1 ;
  int arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (int)jarg1; 
  arg2 = (int)jarg2; 
  result = (pj_status_t)pjsua_set_snd_dev(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_set_1null_1snd_1dev(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  result = (pj_status_t)pjsua_set_null_snd_dev();
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_set_1no_1snd_1dev(JNIEnv *jenv, jclass jcls) {
  jlong jresult = 0 ;
  pjmedia_port *result = 0 ;
  
  (void)jenv;
  (void)jcls;
  result = (pjmedia_port *)pjsua_set_no_snd_dev();
  *(pjmedia_port **)&jresult = result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_set_1ec(JNIEnv *jenv, jclass jcls, jlong jarg1, jlong jarg2) {
  jint jresult = 0 ;
  unsigned int arg1 ;
  unsigned int arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = (unsigned int)jarg1; 
  arg2 = (unsigned int)jarg2; 
  result = (pj_status_t)pjsua_set_ec(arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1ec_1tail(JNIEnv *jenv, jclass jcls, jlong jarg1) {
  jint jresult = 0 ;
  unsigned int *arg1 = (unsigned int *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  arg1 = *(unsigned int **)&jarg1; 
  result = (pj_status_t)pjsua_get_ec_tail(arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_enum_1codecs(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlongArray jarg2) {
  jint jresult = 0 ;
  pjsua_codec_info *arg1 ;
  unsigned int *arg2 = (unsigned int *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_codec_info **)&jarg1; 
  {
    if (!jarg2) {
      SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "array null");
      return 0;
    }
    if (jenv->GetArrayLength(jarg2) == 0) {
      SWIG_JavaThrowException(jenv, SWIG_JavaIndexOutOfBoundsException, "Array must contain at least 1 element");
      return 0;
    }
    arg2 = (unsigned int *) jenv->GetLongArrayElements(jarg2, 0); 
  }
  result = (pj_status_t)pjsua_enum_codecs(arg1,arg2);
  jresult = (jint)result; 
  {
    jenv->ReleaseLongArrayElements(jarg2, (jlong *)arg2, 0); 
  }
  
  
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_get_1nbr_1of_1codecs(JNIEnv *jenv, jclass jcls) {
  jint jresult = 0 ;
  pj_status_t status;

  (void)jenv;
  (void)jcls;

  pjsua_codec_info c[32];
  unsigned i, count = PJ_ARRAY_SIZE(c);
  status = pjsua_enum_codecs(c, &count);
  if(status == PJ_SUCCESS){
	jresult = (jint) count;
  }
  return jresult;
}

SWIGEXPORT jlong JNICALL Java_org_pjsip_pjsua_pjsuaJNI_codec_1get_1id(JNIEnv *jenv, jclass jcls, jint jarg1) {
  pjsua_codec_info c[32];
  unsigned i, count = PJ_ARRAY_SIZE(c);
  int arg1 = 0;
  jlong jresult = 0 ;
  pj_str_t *result = 0 ;

  (void)jenv;
  (void)jcls;
  arg1 = (int)jarg1;

  pjsua_enum_codecs(c, &count);
  if (arg1 < count){
	  result = (pj_str_t *)& (c[arg1].codec_id);
  }
  //TODO : else

  *(pj_str_t **)&jresult = result;
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_codec_1set_1priority(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jshort jarg2) {
  jint jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  pj_uint8_t arg2 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = (pj_uint8_t)jarg2; 
  result = (pj_status_t)pjsua_codec_set_priority((pj_str_t const *)arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_codec_1get_1param(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  pjmedia_codec_param *arg2 = (pjmedia_codec_param *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = *(pjmedia_codec_param **)&jarg2; 
  result = (pj_status_t)pjsua_codec_get_param((pj_str_t const *)arg1,arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_codec_1set_1param(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_, jlong jarg2, jobject jarg2_) {
  jint jresult = 0 ;
  pj_str_t *arg1 = (pj_str_t *) 0 ;
  pjmedia_codec_param *arg2 = (pjmedia_codec_param *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  (void)jarg2_;
  arg1 = *(pj_str_t **)&jarg1; 
  arg2 = *(pjmedia_codec_param **)&jarg2; 
  result = (pj_status_t)pjsua_codec_set_param((pj_str_t const *)arg1,(pjmedia_codec_param const *)arg2);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT jint JNICALL Java_org_pjsip_pjsua_pjsuaJNI_media_1transports_1create(JNIEnv *jenv, jclass jcls, jlong jarg1, jobject jarg1_) {
  jint jresult = 0 ;
  pjsua_transport_config *arg1 = (pjsua_transport_config *) 0 ;
  pj_status_t result;
  
  (void)jenv;
  (void)jcls;
  (void)jarg1_;
  arg1 = *(pjsua_transport_config **)&jarg1; 
  result = (pj_status_t)pjsua_media_transports_create((pjsua_transport_config const *)arg1);
  jresult = (jint)result; 
  return jresult;
}


SWIGEXPORT void JNICALL Java_org_pjsip_pjsua_pjsuaJNI_swig_1module_1init(JNIEnv *jenv, jclass jcls) {
  int i;
  
  static struct {
    const char *method;
    const char *signature;
  } methods[19] = {
    {
      "SwigDirector_Callback_on_call_state", "(Lorg/pjsip/pjsua/Callback;IJ)V" 
    },
    {
      "SwigDirector_Callback_on_incoming_call", "(Lorg/pjsip/pjsua/Callback;IIJ)V" 
    },
    {
      "SwigDirector_Callback_on_call_tsx_state", "(Lorg/pjsip/pjsua/Callback;IJJ)V" 
    },
    {
      "SwigDirector_Callback_on_call_media_state", "(Lorg/pjsip/pjsua/Callback;I)V" 
    },
    {
      "SwigDirector_Callback_on_stream_created", "(Lorg/pjsip/pjsua/Callback;IJJJ)V" 
    },
    {
      "SwigDirector_Callback_on_stream_destroyed", "(Lorg/pjsip/pjsua/Callback;IJJ)V" 
    },
    {
      "SwigDirector_Callback_on_dtmf_digit", "(Lorg/pjsip/pjsua/Callback;II)V" 
    },
    {
      "SwigDirector_Callback_on_call_transfer_request", "(Lorg/pjsip/pjsua/Callback;IJJ)V" 
    },
    {
      "SwigDirector_Callback_on_call_transfer_status", "(Lorg/pjsip/pjsua/Callback;IIJIJ)V" 
    },
    {
      "SwigDirector_Callback_on_call_replace_request", "(Lorg/pjsip/pjsua/Callback;IJJJ)V" 
    },
    {
      "SwigDirector_Callback_on_call_replaced", "(Lorg/pjsip/pjsua/Callback;II)V" 
    },
    {
      "SwigDirector_Callback_on_reg_state", "(Lorg/pjsip/pjsua/Callback;I)V" 
    },
    {
      "SwigDirector_Callback_on_buddy_state", "(Lorg/pjsip/pjsua/Callback;I)V" 
    },
    {
      "SwigDirector_Callback_on_pager", "(Lorg/pjsip/pjsua/Callback;IJJJJJ)V" 
    },
    {
      "SwigDirector_Callback_on_pager2", "(Lorg/pjsip/pjsua/Callback;IJJJJJJ)V" 
    },
    {
      "SwigDirector_Callback_on_pager_status", "(Lorg/pjsip/pjsua/Callback;IJJIJ)V" 
    },
    {
      "SwigDirector_Callback_on_pager_status2", "(Lorg/pjsip/pjsua/Callback;IJJIJJJ)V" 
    },
    {
      "SwigDirector_Callback_on_typing", "(Lorg/pjsip/pjsua/Callback;IJJJI)V" 
    },
    {
      "SwigDirector_Callback_on_nat_detect", "(Lorg/pjsip/pjsua/Callback;J)V" 
    }
  };
  Swig::jclass_pjsuaJNI = (jclass) jenv->NewGlobalRef(jcls);
  if (!Swig::jclass_pjsuaJNI) return;
  for (i = 0; i < (int) (sizeof(methods)/sizeof(methods[0])); ++i) {
    Swig::director_methids[i] = jenv->GetStaticMethodID(jcls, methods[i].method, methods[i].signature);
    if (!Swig::director_methids[i]) return;
  }
}


JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
	JNIEnv *env;
	jclass k;
	jint r;
	//Assume it is c++
	r = vm->GetEnv ((void **) &env, JNI_VERSION_1_4);
	k = env->FindClass ("org/pjsip/pjsua/pjsuaJNI");

#if USE_JNI_AUDIO==1
	android_jvm = vm;
#endif


	//#define NBR_JNI_METHODS 816
	//#define NBR_JNI_METHODS 806
	JNINativeMethod methods[] = {
			{"pj_str_copy", "(Ljava/lang/String;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1copy},
			{"get_snd_dev_info", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1snd_1dev_1info},
			{"delete_Callback", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1Callback},
			{"Callback_on_call_state", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pjsip_event;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1state},
			{"Callback_on_call_stateSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pjsip_event;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1stateSwigExplicitCallback},
			{"Callback_on_incoming_call", "(JLorg/pjsip/pjsua/Callback;IIJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1incoming_1call},
			{"Callback_on_incoming_callSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IIJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1incoming_1callSwigExplicitCallback},
			{"Callback_on_call_tsx_state", "(JLorg/pjsip/pjsua/Callback;IJJLorg/pjsip/pjsua/pjsip_event;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1tsx_1state},
			{"Callback_on_call_tsx_stateSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJJLorg/pjsip/pjsua/pjsip_event;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1tsx_1stateSwigExplicitCallback},
			{"Callback_on_call_media_state", "(JLorg/pjsip/pjsua/Callback;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1media_1state},
			{"Callback_on_call_media_stateSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1media_1stateSwigExplicitCallback},
			{"Callback_on_stream_created", "(JLorg/pjsip/pjsua/Callback;IJJJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1created},
			{"Callback_on_stream_createdSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJJJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1createdSwigExplicitCallback},
			{"Callback_on_stream_destroyed", "(JLorg/pjsip/pjsua/Callback;IJJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1destroyed},
			{"Callback_on_stream_destroyedSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1stream_1destroyedSwigExplicitCallback},
			{"Callback_on_dtmf_digit", "(JLorg/pjsip/pjsua/Callback;II)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1dtmf_1digit},
			{"Callback_on_dtmf_digitSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;II)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1dtmf_1digitSwigExplicitCallback},
			{"Callback_on_call_transfer_request", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1request},
			{"Callback_on_call_transfer_requestSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1requestSwigExplicitCallback},
			{"Callback_on_call_transfer_status", "(JLorg/pjsip/pjsua/Callback;IIJLorg/pjsip/pjsua/pj_str_t;IJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1status},
			{"Callback_on_call_transfer_statusSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IIJLorg/pjsip/pjsua/pj_str_t;IJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1transfer_1statusSwigExplicitCallback},
			{"Callback_on_call_replace_request", "(JLorg/pjsip/pjsua/Callback;IJJJLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replace_1request},
			{"Callback_on_call_replace_requestSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJJJLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replace_1requestSwigExplicitCallback},
			{"Callback_on_call_replaced", "(JLorg/pjsip/pjsua/Callback;II)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replaced},
			{"Callback_on_call_replacedSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;II)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1call_1replacedSwigExplicitCallback},
			{"Callback_on_reg_state", "(JLorg/pjsip/pjsua/Callback;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1reg_1state},
			{"Callback_on_reg_stateSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1reg_1stateSwigExplicitCallback},
			{"Callback_on_buddy_state", "(JLorg/pjsip/pjsua/Callback;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1buddy_1state},
			{"Callback_on_buddy_stateSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1buddy_1stateSwigExplicitCallback},
			{"Callback_on_pager", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager},
			{"Callback_on_pagerSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pagerSwigExplicitCallback},
			{"Callback_on_pager2", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager2},
			{"Callback_on_pager2SwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager2SwigExplicitCallback},
			{"Callback_on_pager_status", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;IJLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1status},
			{"Callback_on_pager_statusSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;IJLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1statusSwigExplicitCallback},
			{"Callback_on_pager_status2", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;IJLorg/pjsip/pjsua/pj_str_t;JJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1status2},
			{"Callback_on_pager_status2SwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;IJLorg/pjsip/pjsua/pj_str_t;JJ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1pager_1status2SwigExplicitCallback},
			{"Callback_on_typing", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1typing},
			{"Callback_on_typingSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1typingSwigExplicitCallback},
			{"Callback_on_nat_detect", "(JLorg/pjsip/pjsua/Callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1nat_1detect},
			{"Callback_on_nat_detectSwigExplicitCallback", "(JLorg/pjsip/pjsua/Callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1on_1nat_1detectSwigExplicitCallback},
			{"new_Callback", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1Callback},
			{"Callback_director_connect", "(Lorg/pjsip/pjsua/Callback;JZZ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1director_1connect},
			{"Callback_change_ownership", "(Lorg/pjsip/pjsua/Callback;JZ)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_Callback_1change_1ownership},
			{"WRAPPER_CALLBACK_STRUCT_get", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_WRAPPER_1CALLBACK_1STRUCT_1get},
			{"setCallbackObject", "(JLorg/pjsip/pjsua/Callback;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_setCallbackObject},
			{"PJ_SUCCESS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJ_1SUCCESS_1get},
			{"PJ_TRUE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJ_1TRUE_1get},
			{"PJ_FALSE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJ_1FALSE_1get},
			{"pj_str_t_ptr_set", "(JLorg/pjsip/pjsua/pj_str_t;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1ptr_1set},
			{"pj_str_t_ptr_get", "(JLorg/pjsip/pjsua/pj_str_t;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1ptr_1get},
			{"pj_str_t_slen_set", "(JLorg/pjsip/pjsua/pj_str_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1slen_1set},
			{"pj_str_t_slen_get", "(JLorg/pjsip/pjsua/pj_str_t;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1str_1t_1slen_1get},
			{"new_pj_str_t", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pj_1str_1t},
			{"delete_pj_str_t", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pj_1str_1t},
			{"pjmedia_codec_param_setting_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1get},
			{"pjmedia_codec_param_info_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1get},
			{"new_pjmedia_codec_param", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1codec_1param},
			{"delete_pjmedia_codec_param", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1codec_1param},
			{"pjmedia_codec_param_setting_frm_per_pkt_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1frm_1per_1pkt_1set},
			{"pjmedia_codec_param_setting_frm_per_pkt_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1frm_1per_1pkt_1get},
			{"pjmedia_codec_param_setting_vad_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1vad_1set},
			{"pjmedia_codec_param_setting_vad_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1vad_1get},
			{"pjmedia_codec_param_setting_cng_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1cng_1set},
			{"pjmedia_codec_param_setting_cng_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1cng_1get},
			{"pjmedia_codec_param_setting_penh_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1penh_1set},
			{"pjmedia_codec_param_setting_penh_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1penh_1get},
			{"pjmedia_codec_param_setting_plc_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1plc_1set},
			{"pjmedia_codec_param_setting_plc_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1plc_1get},
			{"pjmedia_codec_param_setting_reserved_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1reserved_1set},
			{"pjmedia_codec_param_setting_reserved_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1reserved_1get},
			{"pjmedia_codec_param_setting_enc_fmtp_mode_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1enc_1fmtp_1mode_1set},
			{"pjmedia_codec_param_setting_enc_fmtp_mode_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1enc_1fmtp_1mode_1get},
			{"pjmedia_codec_param_setting_dec_fmtp_mode_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1dec_1fmtp_1mode_1set},
			{"pjmedia_codec_param_setting_dec_fmtp_mode_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_setting;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1setting_1dec_1fmtp_1mode_1get},
			{"new_pjmedia_codec_param_setting", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1codec_1param_1setting},
			{"delete_pjmedia_codec_param_setting", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1codec_1param_1setting},
			{"pjmedia_codec_param_info_clock_rate_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1clock_1rate_1set},
			{"pjmedia_codec_param_info_clock_rate_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1clock_1rate_1get},
			{"pjmedia_codec_param_info_channel_cnt_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1channel_1cnt_1set},
			{"pjmedia_codec_param_info_channel_cnt_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1channel_1cnt_1get},
			{"pjmedia_codec_param_info_avg_bps_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1avg_1bps_1set},
			{"pjmedia_codec_param_info_avg_bps_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1avg_1bps_1get},
			{"pjmedia_codec_param_info_max_bps_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1max_1bps_1set},
			{"pjmedia_codec_param_info_max_bps_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1max_1bps_1get},
			{"pjmedia_codec_param_info_frm_ptime_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1frm_1ptime_1set},
			{"pjmedia_codec_param_info_frm_ptime_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1frm_1ptime_1get},
			{"pjmedia_codec_param_info_enc_ptime_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1enc_1ptime_1set},
			{"pjmedia_codec_param_info_enc_ptime_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1enc_1ptime_1get},
			{"pjmedia_codec_param_info_pcm_bits_per_sample_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pcm_1bits_1per_1sample_1set},
			{"pjmedia_codec_param_info_pcm_bits_per_sample_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pcm_1bits_1per_1sample_1get},
			{"pjmedia_codec_param_info_pt_set", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pt_1set},
			{"pjmedia_codec_param_info_pt_get", "(JLorg/pjsip/pjsua/pjmedia_codec_param_info;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1codec_1param_1info_1pt_1get},
			{"new_pjmedia_codec_param_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1codec_1param_1info},
			{"delete_pjmedia_codec_param_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1codec_1param_1info},
			{"PJMEDIA_DIR_NONE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1NONE_1get},
			{"PJMEDIA_DIR_ENCODING_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1ENCODING_1get},
			{"PJMEDIA_DIR_DECODING_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1DECODING_1get},
			{"PJMEDIA_DIR_ENCODING_DECODING_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1DIR_1ENCODING_1DECODING_1get},
			{"pjmedia_port_info_name_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1name_1set},
			{"pjmedia_port_info_name_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1name_1get},
			{"pjmedia_port_info_signature_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1signature_1set},
			{"pjmedia_port_info_signature_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1signature_1get},
			{"pjmedia_port_info_type_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1type_1set},
			{"pjmedia_port_info_type_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1type_1get},
			{"pjmedia_port_info_has_info_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1has_1info_1set},
			{"pjmedia_port_info_has_info_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1has_1info_1get},
			{"pjmedia_port_info_need_info_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1need_1info_1set},
			{"pjmedia_port_info_need_info_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1need_1info_1get},
			{"pjmedia_port_info_pt_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1pt_1set},
			{"pjmedia_port_info_pt_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1pt_1get},
			{"pjmedia_port_info_encoding_name_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1encoding_1name_1set},
			{"pjmedia_port_info_encoding_name_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1encoding_1name_1get},
			{"pjmedia_port_info_clock_rate_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1clock_1rate_1set},
			{"pjmedia_port_info_clock_rate_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1clock_1rate_1get},
			{"pjmedia_port_info_channel_count_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1channel_1count_1set},
			{"pjmedia_port_info_channel_count_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1channel_1count_1get},
			{"pjmedia_port_info_bits_per_sample_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bits_1per_1sample_1set},
			{"pjmedia_port_info_bits_per_sample_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bits_1per_1sample_1get},
			{"pjmedia_port_info_samples_per_frame_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1samples_1per_1frame_1set},
			{"pjmedia_port_info_samples_per_frame_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1samples_1per_1frame_1get},
			{"pjmedia_port_info_bytes_per_frame_set", "(JLorg/pjsip/pjsua/pjmedia_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bytes_1per_1frame_1set},
			{"pjmedia_port_info_bytes_per_frame_get", "(JLorg/pjsip/pjsua/pjmedia_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1bytes_1per_1frame_1get},
			{"new_pjmedia_port_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1port_1info},
			{"delete_pjmedia_port_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1port_1info},
			{"pjmedia_port_info_set", "(JLorg/pjsip/pjsua/pjmedia_port;JLorg/pjsip/pjsua/pjmedia_port_info;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1set},
			{"pjmedia_port_info_get", "(JLorg/pjsip/pjsua/pjmedia_port;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1info_1get},
			{"pjmedia_port_put_frame_set", "(JLorg/pjsip/pjsua/pjmedia_port;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1put_1frame_1set},
			{"pjmedia_port_put_frame_get", "(JLorg/pjsip/pjsua/pjmedia_port;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1put_1frame_1get},
			{"pjmedia_port_get_frame_set", "(JLorg/pjsip/pjsua/pjmedia_port;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1get_1frame_1set},
			{"pjmedia_port_get_frame_get", "(JLorg/pjsip/pjsua/pjmedia_port;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1get_1frame_1get},
			{"pjmedia_port_on_destroy_set", "(JLorg/pjsip/pjsua/pjmedia_port;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1on_1destroy_1set},
			{"pjmedia_port_on_destroy_get", "(JLorg/pjsip/pjsua/pjmedia_port;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1on_1destroy_1get},
			{"pjmedia_port_port_data_get", "(JLorg/pjsip/pjsua/pjmedia_port;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1get},
			{"pjmedia_port_port_data_pdata_set", "(JLorg/pjsip/pjsua/pjmedia_port_port_data;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1pdata_1set},
			{"pjmedia_port_port_data_ldata_set", "(JLorg/pjsip/pjsua/pjmedia_port_port_data;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1ldata_1set},
			{"pjmedia_port_port_data_ldata_get", "(JLorg/pjsip/pjsua/pjmedia_port_port_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1port_1port_1data_1ldata_1get},
			{"new_pjmedia_port_port_data", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1port_1port_1data},
			{"delete_pjmedia_port_port_data", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1port_1port_1data},
			{"pjsip_cred_info_realm_set", "(JLorg/pjsip/pjsua/pjsip_cred_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1realm_1set},
			{"pjsip_cred_info_realm_get", "(JLorg/pjsip/pjsua/pjsip_cred_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1realm_1get},
			{"pjsip_cred_info_scheme_set", "(JLorg/pjsip/pjsua/pjsip_cred_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1scheme_1set},
			{"pjsip_cred_info_scheme_get", "(JLorg/pjsip/pjsua/pjsip_cred_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1scheme_1get},
			{"pjsip_cred_info_username_set", "(JLorg/pjsip/pjsua/pjsip_cred_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1username_1set},
			{"pjsip_cred_info_username_get", "(JLorg/pjsip/pjsua/pjsip_cred_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1username_1get},
			{"pjsip_cred_info_data_type_set", "(JLorg/pjsip/pjsua/pjsip_cred_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1type_1set},
			{"pjsip_cred_info_data_type_get", "(JLorg/pjsip/pjsua/pjsip_cred_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1type_1get},
			{"pjsip_cred_info_data_set", "(JLorg/pjsip/pjsua/pjsip_cred_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1set},
			{"pjsip_cred_info_data_get", "(JLorg/pjsip/pjsua/pjsip_cred_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1data_1get},
			{"pjsip_cred_info_ext_get", "(JLorg/pjsip/pjsua/pjsip_cred_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1get},
			{"new_pjsip_cred_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1cred_1info},
			{"delete_pjsip_cred_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1cred_1info},
			{"pjsip_cred_info_ext_aka_get", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1get},
			{"new_pjsip_cred_info_ext", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1cred_1info_1ext},
			{"delete_pjsip_cred_info_ext", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1cred_1info_1ext},
			{"pjsip_cred_info_ext_aka_k_set", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1k_1set},
			{"pjsip_cred_info_ext_aka_k_get", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1k_1get},
			{"pjsip_cred_info_ext_aka_op_set", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1op_1set},
			{"pjsip_cred_info_ext_aka_op_get", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1op_1get},
			{"pjsip_cred_info_ext_aka_amf_set", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1amf_1set},
			{"pjsip_cred_info_ext_aka_amf_get", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1amf_1get},
			{"pjsip_cred_info_ext_aka_cb_set", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1cb_1set},
			{"pjsip_cred_info_ext_aka_cb_get", "(JLorg/pjsip/pjsua/pjsip_cred_info_ext_aka;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1cred_1info_1ext_1aka_1cb_1get},
			{"new_pjsip_cred_info_ext_aka", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1cred_1info_1ext_1aka},
			{"delete_pjsip_cred_info_ext_aka", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1cred_1info_1ext_1aka},
			{"PJSIP_CRED_DATA_PLAIN_PASSWD_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1CRED_1DATA_1PLAIN_1PASSWD_1get},
			{"PJSIP_CRED_DATA_DIGEST_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1CRED_1DATA_1DIGEST_1get},
			{"PJSIP_CRED_DATA_EXT_AKA_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1CRED_1DATA_1EXT_1AKA_1get},
			{"PJMEDIA_TONEGEN_LOOP_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1TONEGEN_1LOOP_1get},
			{"PJMEDIA_TONEGEN_NO_LOCK_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJMEDIA_1TONEGEN_1NO_1LOCK_1get},
			{"pjsip_event_type_set", "(JLorg/pjsip/pjsua/pjsip_event;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1type_1set},
			{"pjsip_event_type_get", "(JLorg/pjsip/pjsua/pjsip_event;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1type_1get},
			{"pjsip_event_body_get", "(JLorg/pjsip/pjsua/pjsip_event;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1get},
			{"new_pjsip_event", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event},
			{"delete_pjsip_event", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event},
			{"pjsip_event_body_user_get", "(JLorg/pjsip/pjsua/pjsip_event_body;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1get},
			{"pjsip_event_body_rx_msg_get", "(JLorg/pjsip/pjsua/pjsip_event_body;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1rx_1msg_1get},
			{"pjsip_event_body_tx_error_get", "(JLorg/pjsip/pjsua/pjsip_event_body;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1get},
			{"pjsip_event_body_tx_msg_get", "(JLorg/pjsip/pjsua/pjsip_event_body;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1msg_1get},
			{"pjsip_event_body_tsx_state_get", "(JLorg/pjsip/pjsua/pjsip_event_body;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1get},
			{"pjsip_event_body_timer_get", "(JLorg/pjsip/pjsua/pjsip_event_body;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1timer_1get},
			{"new_pjsip_event_body", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body},
			{"delete_pjsip_event_body", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body},
			{"pjsip_event_body_user_user1_set", "(JLorg/pjsip/pjsua/pjsip_event_body_user;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user1_1set},
			{"pjsip_event_body_user_user2_set", "(JLorg/pjsip/pjsua/pjsip_event_body_user;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user2_1set},
			{"pjsip_event_body_user_user3_set", "(JLorg/pjsip/pjsua/pjsip_event_body_user;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user3_1set},
			{"pjsip_event_body_user_user4_set", "(JLorg/pjsip/pjsua/pjsip_event_body_user;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1user_1user4_1set},
			{"new_pjsip_event_body_user", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1user},
			{"delete_pjsip_event_body_user", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1user},
			{"pjsip_event_body_rx_msg_rdata_set", "(JLorg/pjsip/pjsua/pjsip_event_body_rx_msg;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1rx_1msg_1rdata_1set},
			{"pjsip_event_body_rx_msg_rdata_get", "(JLorg/pjsip/pjsua/pjsip_event_body_rx_msg;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1rx_1msg_1rdata_1get},
			{"new_pjsip_event_body_rx_msg", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1rx_1msg},
			{"delete_pjsip_event_body_rx_msg", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1rx_1msg},
			{"pjsip_event_body_tx_error_tdata_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tx_error;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tdata_1set},
			{"pjsip_event_body_tx_error_tdata_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tx_error;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tdata_1get},
			{"pjsip_event_body_tx_error_tsx_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tx_error;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tsx_1set},
			{"pjsip_event_body_tx_error_tsx_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tx_error;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1error_1tsx_1get},
			{"new_pjsip_event_body_tx_error", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tx_1error},
			{"delete_pjsip_event_body_tx_error", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tx_1error},
			{"pjsip_event_body_tx_msg_tdata_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tx_msg;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1msg_1tdata_1set},
			{"pjsip_event_body_tx_msg_tdata_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tx_msg;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tx_1msg_1tdata_1get},
			{"new_pjsip_event_body_tx_msg", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tx_1msg},
			{"delete_pjsip_event_body_tx_msg", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tx_1msg},
			{"pjsip_event_body_tsx_state_tsx_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1tsx_1set},
			{"pjsip_event_body_tsx_state_tsx_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1tsx_1get},
			{"pjsip_event_body_tsx_state_prev_state_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1prev_1state_1set},
			{"pjsip_event_body_tsx_state_prev_state_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1prev_1state_1get},
			{"pjsip_event_body_tsx_state_type_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1type_1set},
			{"pjsip_event_body_tsx_state_type_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1type_1get},
			{"pjsip_event_body_tsx_state_src_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1get},
			{"new_pjsip_event_body_tsx_state", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tsx_1state},
			{"delete_pjsip_event_body_tsx_state", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tsx_1state},
			{"pjsip_event_body_tsx_state_src_rdata_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1rdata_1set},
			{"pjsip_event_body_tsx_state_src_rdata_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1rdata_1get},
			{"pjsip_event_body_tsx_state_src_tdata_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1tdata_1set},
			{"pjsip_event_body_tsx_state_src_tdata_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1tdata_1get},
			{"pjsip_event_body_tsx_state_src_timer_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1timer_1set},
			{"pjsip_event_body_tsx_state_src_timer_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1timer_1get},
			{"pjsip_event_body_tsx_state_src_status_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1status_1set},
			{"pjsip_event_body_tsx_state_src_status_get", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1status_1get},
			{"pjsip_event_body_tsx_state_src_data_set", "(JLorg/pjsip/pjsua/pjsip_event_body_tsx_state_src;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1tsx_1state_1src_1data_1set},
			{"new_pjsip_event_body_tsx_state_src", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1tsx_1state_1src},
			{"delete_pjsip_event_body_tsx_state_src", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1tsx_1state_1src},
			{"pjsip_event_body_timer_entry_set", "(JLorg/pjsip/pjsua/pjsip_event_body_timer;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1timer_1entry_1set},
			{"pjsip_event_body_timer_entry_get", "(JLorg/pjsip/pjsua/pjsip_event_body_timer;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1event_1body_1timer_1entry_1get},
			{"new_pjsip_event_body_timer", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsip_1event_1body_1timer},
			{"delete_pjsip_event_body_timer", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsip_1event_1body_1timer},
			{"pjmedia_snd_dev_info_name_set", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1name_1set},
			{"pjmedia_snd_dev_info_name_get", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1name_1get},
			{"pjmedia_snd_dev_info_input_count_set", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1input_1count_1set},
			{"pjmedia_snd_dev_info_input_count_get", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1input_1count_1get},
			{"pjmedia_snd_dev_info_output_count_set", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1output_1count_1set},
			{"pjmedia_snd_dev_info_output_count_get", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1output_1count_1get},
			{"pjmedia_snd_dev_info_default_samples_per_sec_set", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1default_1samples_1per_1sec_1set},
			{"pjmedia_snd_dev_info_default_samples_per_sec_get", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1dev_1info_1default_1samples_1per_1sec_1get},
			{"new_pjmedia_snd_dev_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1snd_1dev_1info},
			{"delete_pjmedia_snd_dev_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1snd_1dev_1info},
			{"pjmedia_tone_desc_freq1_set", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq1_1set},
			{"pjmedia_tone_desc_freq1_get", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq1_1get},
			{"pjmedia_tone_desc_freq2_set", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq2_1set},
			{"pjmedia_tone_desc_freq2_get", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1freq2_1get},
			{"pjmedia_tone_desc_on_msec_set", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1on_1msec_1set},
			{"pjmedia_tone_desc_on_msec_get", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1on_1msec_1get},
			{"pjmedia_tone_desc_off_msec_set", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1off_1msec_1set},
			{"pjmedia_tone_desc_off_msec_get", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1off_1msec_1get},
			{"pjmedia_tone_desc_volume_set", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1volume_1set},
			{"pjmedia_tone_desc_volume_get", "(JLorg/pjsip/pjsua/pjmedia_tone_desc;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tone_1desc_1volume_1get},
			{"new_pjmedia_tone_desc", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjmedia_1tone_1desc},
			{"delete_pjmedia_tone_desc", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjmedia_1tone_1desc},
			{"pj_pool_t_obj_name_set", "(JLorg/pjsip/pjsua/pj_pool_t;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1obj_1name_1set},
			{"pj_pool_t_obj_name_get", "(JLorg/pjsip/pjsua/pj_pool_t;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1obj_1name_1get},
			{"pj_pool_t_factory_set", "(JLorg/pjsip/pjsua/pj_pool_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1set},
			{"pj_pool_t_factory_get", "(JLorg/pjsip/pjsua/pj_pool_t;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1get},
			{"pj_pool_t_factory_data_set", "(JLorg/pjsip/pjsua/pj_pool_t;[B)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1factory_1data_1set},
			{"pj_pool_t_capacity_set", "(JLorg/pjsip/pjsua/pj_pool_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1capacity_1set},
			{"pj_pool_t_capacity_get", "(JLorg/pjsip/pjsua/pj_pool_t;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1capacity_1get},
			{"pj_pool_t_increment_size_set", "(JLorg/pjsip/pjsua/pj_pool_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1increment_1size_1set},
			{"pj_pool_t_increment_size_get", "(JLorg/pjsip/pjsua/pj_pool_t;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1increment_1size_1get},
			{"pj_pool_t_block_list_set", "(JLorg/pjsip/pjsua/pj_pool_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1block_1list_1set},
			{"pj_pool_t_block_list_get", "(JLorg/pjsip/pjsua/pj_pool_t;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1block_1list_1get},
			{"pj_pool_t_callback_set", "(JLorg/pjsip/pjsua/pj_pool_t;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1callback_1set},
			{"pj_pool_t_callback_get", "(JLorg/pjsip/pjsua/pj_pool_t;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1t_1callback_1get},
			{"new_pj_pool_t", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pj_1pool_1t},
			{"delete_pj_pool_t", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pj_1pool_1t},
			{"PJSIP_TRANSPORT_IPV6_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1TRANSPORT_1IPV6_1get},
			{"PJSIP_TRANSPORT_UDP6_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1TRANSPORT_1UDP6_1get},
			{"PJSIP_TRANSPORT_TCP6_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1TRANSPORT_1TCP6_1get},
			{"PJSIP_SC_TRYING_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TRYING_1get},
			{"PJSIP_SC_RINGING_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1RINGING_1get},
			{"PJSIP_SC_CALL_BEING_FORWARDED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1CALL_1BEING_1FORWARDED_1get},
			{"PJSIP_SC_QUEUED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1QUEUED_1get},
			{"PJSIP_SC_PROGRESS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PROGRESS_1get},
			{"PJSIP_SC_OK_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1OK_1get},
			{"PJSIP_SC_ACCEPTED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1ACCEPTED_1get},
			{"PJSIP_SC_MULTIPLE_CHOICES_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MULTIPLE_1CHOICES_1get},
			{"PJSIP_SC_MOVED_PERMANENTLY_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MOVED_1PERMANENTLY_1get},
			{"PJSIP_SC_MOVED_TEMPORARILY_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MOVED_1TEMPORARILY_1get},
			{"PJSIP_SC_USE_PROXY_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1USE_1PROXY_1get},
			{"PJSIP_SC_ALTERNATIVE_SERVICE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1ALTERNATIVE_1SERVICE_1get},
			{"PJSIP_SC_BAD_REQUEST_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1REQUEST_1get},
			{"PJSIP_SC_UNAUTHORIZED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNAUTHORIZED_1get},
			{"PJSIP_SC_PAYMENT_REQUIRED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PAYMENT_1REQUIRED_1get},
			{"PJSIP_SC_FORBIDDEN_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1FORBIDDEN_1get},
			{"PJSIP_SC_NOT_FOUND_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1FOUND_1get},
			{"PJSIP_SC_METHOD_NOT_ALLOWED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1METHOD_1NOT_1ALLOWED_1get},
			{"PJSIP_SC_NOT_ACCEPTABLE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1ACCEPTABLE_1get},
			{"PJSIP_SC_PROXY_AUTHENTICATION_REQUIRED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PROXY_1AUTHENTICATION_1REQUIRED_1get},
			{"PJSIP_SC_REQUEST_TIMEOUT_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1TIMEOUT_1get},
			{"PJSIP_SC_GONE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1GONE_1get},
			{"PJSIP_SC_REQUEST_ENTITY_TOO_LARGE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1ENTITY_1TOO_1LARGE_1get},
			{"PJSIP_SC_REQUEST_URI_TOO_LONG_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1URI_1TOO_1LONG_1get},
			{"PJSIP_SC_UNSUPPORTED_MEDIA_TYPE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNSUPPORTED_1MEDIA_1TYPE_1get},
			{"PJSIP_SC_UNSUPPORTED_URI_SCHEME_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNSUPPORTED_1URI_1SCHEME_1get},
			{"PJSIP_SC_BAD_EXTENSION_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1EXTENSION_1get},
			{"PJSIP_SC_EXTENSION_REQUIRED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1EXTENSION_1REQUIRED_1get},
			{"PJSIP_SC_SESSION_TIMER_TOO_SMALL_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1SESSION_1TIMER_1TOO_1SMALL_1get},
			{"PJSIP_SC_INTERVAL_TOO_BRIEF_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1INTERVAL_1TOO_1BRIEF_1get},
			{"PJSIP_SC_TEMPORARILY_UNAVAILABLE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TEMPORARILY_1UNAVAILABLE_1get},
			{"PJSIP_SC_CALL_TSX_DOES_NOT_EXIST_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1CALL_1TSX_1DOES_1NOT_1EXIST_1get},
			{"PJSIP_SC_LOOP_DETECTED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1LOOP_1DETECTED_1get},
			{"PJSIP_SC_TOO_MANY_HOPS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TOO_1MANY_1HOPS_1get},
			{"PJSIP_SC_ADDRESS_INCOMPLETE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1ADDRESS_1INCOMPLETE_1get},
			{"PJSIP_AC_AMBIGUOUS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1AC_1AMBIGUOUS_1get},
			{"PJSIP_SC_BUSY_HERE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BUSY_1HERE_1get},
			{"PJSIP_SC_REQUEST_TERMINATED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1TERMINATED_1get},
			{"PJSIP_SC_NOT_ACCEPTABLE_HERE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1ACCEPTABLE_1HERE_1get},
			{"PJSIP_SC_BAD_EVENT_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1EVENT_1get},
			{"PJSIP_SC_REQUEST_UPDATED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1UPDATED_1get},
			{"PJSIP_SC_REQUEST_PENDING_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1REQUEST_1PENDING_1get},
			{"PJSIP_SC_UNDECIPHERABLE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1UNDECIPHERABLE_1get},
			{"PJSIP_SC_INTERNAL_SERVER_ERROR_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1INTERNAL_1SERVER_1ERROR_1get},
			{"PJSIP_SC_NOT_IMPLEMENTED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1IMPLEMENTED_1get},
			{"PJSIP_SC_BAD_GATEWAY_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BAD_1GATEWAY_1get},
			{"PJSIP_SC_SERVICE_UNAVAILABLE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1SERVICE_1UNAVAILABLE_1get},
			{"PJSIP_SC_SERVER_TIMEOUT_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1SERVER_1TIMEOUT_1get},
			{"PJSIP_SC_VERSION_NOT_SUPPORTED_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1VERSION_1NOT_1SUPPORTED_1get},
			{"PJSIP_SC_MESSAGE_TOO_LARGE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1MESSAGE_1TOO_1LARGE_1get},
			{"PJSIP_SC_PRECONDITION_FAILURE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1PRECONDITION_1FAILURE_1get},
			{"PJSIP_SC_BUSY_EVERYWHERE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1BUSY_1EVERYWHERE_1get},
			{"PJSIP_SC_DECLINE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1DECLINE_1get},
			{"PJSIP_SC_DOES_NOT_EXIST_ANYWHERE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1DOES_1NOT_1EXIST_1ANYWHERE_1get},
			{"PJSIP_SC_NOT_ACCEPTABLE_ANYWHERE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1NOT_1ACCEPTABLE_1ANYWHERE_1get},
			{"PJSIP_SC_TSX_TIMEOUT_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TSX_1TIMEOUT_1get},
			{"PJSIP_SC_TSX_TRANSPORT_ERROR_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSIP_1SC_1TSX_1TRANSPORT_1ERROR_1get},
			{"pjsua_pool_create", "(Ljava/lang/String;JJ)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1pool_1create},
			{"pj_pool_release", "(JLorg/pjsip/pjsua/pj_pool_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pj_1pool_1release},
			{"snd_get_dev_count", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_snd_1get_1dev_1count},
			{"pjmedia_snd_set_latency", "(JJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1snd_1set_1latency},
			{"pjmedia_tonegen_create2", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pj_str_t;JJJJJLorg/pjsip/pjsua/pjmedia_port;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tonegen_1create2},
			{"pjmedia_tonegen_play", "(JLorg/pjsip/pjsua/pjmedia_port;J[JJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tonegen_1play},
	//		{"pjmedia_tonegen_rewind", "(JLorg/pjsip/pjsua/pjmedia_port;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjmedia_1tonegen_1rewind},
			{"PJSUA_INVALID_ID_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1INVALID_1ID_1get},
			{"PJSUA_ACC_MAX_PROXIES_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1ACC_1MAX_1PROXIES_1get},
			{"pjsua_logging_config_msg_logging_set", "(JLorg/pjsip/pjsua/pjsua_logging_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1msg_1logging_1set},
	//		{"pjsua_logging_config_msg_logging_get", "(JLorg/pjsip/pjsua/pjsua_logging_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1msg_1logging_1get},
			{"pjsua_logging_config_level_set", "(JLorg/pjsip/pjsua/pjsua_logging_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1level_1set},
	//		{"pjsua_logging_config_level_get", "(JLorg/pjsip/pjsua/pjsua_logging_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1level_1get},
			{"pjsua_logging_config_console_level_set", "(JLorg/pjsip/pjsua/pjsua_logging_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1console_1level_1set},
	//		{"pjsua_logging_config_console_level_get", "(JLorg/pjsip/pjsua/pjsua_logging_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1console_1level_1get},
	//		{"pjsua_logging_config_decor_set", "(JLorg/pjsip/pjsua/pjsua_logging_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1decor_1set},
	//		{"pjsua_logging_config_decor_get", "(JLorg/pjsip/pjsua/pjsua_logging_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1decor_1get},
	//		{"pjsua_logging_config_log_filename_set", "(JLorg/pjsip/pjsua/pjsua_logging_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1log_1filename_1set},
	//		{"pjsua_logging_config_log_filename_get", "(JLorg/pjsip/pjsua/pjsua_logging_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1log_1filename_1get},
	//		{"pjsua_logging_config_cb_set", "(JLorg/pjsip/pjsua/pjsua_logging_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1cb_1set},
	//		{"pjsua_logging_config_cb_get", "(JLorg/pjsip/pjsua/pjsua_logging_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1logging_1config_1cb_1get},
			{"new_pjsua_logging_config", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1logging_1config},
			{"delete_pjsua_logging_config", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1logging_1config},
			{"logging_config_default", "(JLorg/pjsip/pjsua/pjsua_logging_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_logging_1config_1default},
			{"logging_config_dup", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pjsua_logging_config;JLorg/pjsip/pjsua/pjsua_logging_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_logging_1config_1dup},
			{"pjsua_callback_on_call_state_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1state_1set},
			{"pjsua_callback_on_call_state_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1state_1get},
			{"pjsua_callback_on_incoming_call_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1incoming_1call_1set},
			{"pjsua_callback_on_incoming_call_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1incoming_1call_1get},
			{"pjsua_callback_on_call_tsx_state_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1tsx_1state_1set},
			{"pjsua_callback_on_call_tsx_state_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1tsx_1state_1get},
			{"pjsua_callback_on_call_media_state_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1media_1state_1set},
			{"pjsua_callback_on_call_media_state_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1media_1state_1get},
			{"pjsua_callback_on_stream_created_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1created_1set},
			{"pjsua_callback_on_stream_created_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1created_1get},
			{"pjsua_callback_on_stream_destroyed_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1destroyed_1set},
			{"pjsua_callback_on_stream_destroyed_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1stream_1destroyed_1get},
			{"pjsua_callback_on_dtmf_digit_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1dtmf_1digit_1set},
			{"pjsua_callback_on_dtmf_digit_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1dtmf_1digit_1get},
			{"pjsua_callback_on_call_transfer_request_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1request_1set},
			{"pjsua_callback_on_call_transfer_request_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1request_1get},
			{"pjsua_callback_on_call_transfer_status_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1status_1set},
			{"pjsua_callback_on_call_transfer_status_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1transfer_1status_1get},
			{"pjsua_callback_on_call_replace_request_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replace_1request_1set},
			{"pjsua_callback_on_call_replace_request_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replace_1request_1get},
			{"pjsua_callback_on_call_replaced_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replaced_1set},
			{"pjsua_callback_on_call_replaced_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1call_1replaced_1get},
			{"pjsua_callback_on_reg_state_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1reg_1state_1set},
			{"pjsua_callback_on_reg_state_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1reg_1state_1get},
			{"pjsua_callback_on_buddy_state_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1buddy_1state_1set},
			{"pjsua_callback_on_buddy_state_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1buddy_1state_1get},
			{"pjsua_callback_on_pager_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1set},
			{"pjsua_callback_on_pager_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1get},
			{"pjsua_callback_on_pager2_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager2_1set},
			{"pjsua_callback_on_pager2_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager2_1get},
			{"pjsua_callback_on_pager_status_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status_1set},
			{"pjsua_callback_on_pager_status_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status_1get},
			{"pjsua_callback_on_pager_status2_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status2_1set},
			{"pjsua_callback_on_pager_status2_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1pager_1status2_1get},
			{"pjsua_callback_on_typing_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1typing_1set},
			{"pjsua_callback_on_typing_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1typing_1get},
			{"pjsua_callback_on_nat_detect_set", "(JLorg/pjsip/pjsua/pjsua_callback;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1nat_1detect_1set},
			{"pjsua_callback_on_nat_detect_get", "(JLorg/pjsip/pjsua/pjsua_callback;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1callback_1on_1nat_1detect_1get},
			{"new_pjsua_callback", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1callback},
			{"delete_pjsua_callback", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1callback},
			{"pjsua_config_max_calls_set", "(JLorg/pjsip/pjsua/pjsua_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1max_1calls_1set},
			{"pjsua_config_max_calls_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1max_1calls_1get},
			{"pjsua_config_thread_cnt_set", "(JLorg/pjsip/pjsua/pjsua_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1thread_1cnt_1set},
			{"pjsua_config_thread_cnt_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1thread_1cnt_1get},
			{"pjsua_config_nameserver_count_set", "(JLorg/pjsip/pjsua/pjsua_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1count_1set},
			{"pjsua_config_nameserver_count_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1count_1get},
			{"pjsua_config_nameserver_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1set},
			{"pjsua_config_nameserver_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nameserver_1get},
			{"pjsua_config_outbound_proxy_cnt_set", "(JLorg/pjsip/pjsua/pjsua_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1cnt_1set},
			{"pjsua_config_outbound_proxy_cnt_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1cnt_1get},
			{"pjsua_config_outbound_proxy_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1set},
			{"pjsua_config_outbound_proxy_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1outbound_1proxy_1get},
			{"pjsua_config_stun_domain_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1domain_1set},
			{"pjsua_config_stun_domain_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1domain_1get},
			{"pjsua_config_stun_host_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1host_1set},
			{"pjsua_config_stun_host_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1stun_1host_1get},
			{"pjsua_config_nat_type_in_sdp_set", "(JLorg/pjsip/pjsua/pjsua_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nat_1type_1in_1sdp_1set},
			{"pjsua_config_nat_type_in_sdp_get", "(JLorg/pjsip/pjsua/pjsua_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1nat_1type_1in_1sdp_1get},
			{"pjsua_config_require_100rel_set", "(JLorg/pjsip/pjsua/pjsua_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1require_1100rel_1set},
			{"pjsua_config_require_100rel_get", "(JLorg/pjsip/pjsua/pjsua_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1require_1100rel_1get},
			{"pjsua_config_cred_count_set", "(JLorg/pjsip/pjsua/pjsua_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1count_1set},
			{"pjsua_config_cred_count_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1count_1get},
			{"pjsua_config_cred_info_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pjsip_cred_info;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1info_1set},
			{"pjsua_config_cred_info_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cred_1info_1get},
			{"pjsua_config_cb_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pjsua_callback;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cb_1set},
			{"pjsua_config_cb_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1cb_1get},
			{"pjsua_config_user_agent_set", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1user_1agent_1set},
			{"pjsua_config_user_agent_get", "(JLorg/pjsip/pjsua/pjsua_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1config_1user_1agent_1get},
			{"new_pjsua_config", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1config},
			{"delete_pjsua_config", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1config},
			{"config_default", "(JLorg/pjsip/pjsua/pjsua_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_config_1default},
			{"config_dup", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pjsua_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_config_1dup},
			{"pjsua_msg_data_hdr_list_set", "(JLorg/pjsip/pjsua/pjsua_msg_data;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1hdr_1list_1set},
			{"pjsua_msg_data_hdr_list_get", "(JLorg/pjsip/pjsua/pjsua_msg_data;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1hdr_1list_1get},
			{"pjsua_msg_data_content_type_set", "(JLorg/pjsip/pjsua/pjsua_msg_data;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1content_1type_1set},
			{"pjsua_msg_data_content_type_get", "(JLorg/pjsip/pjsua/pjsua_msg_data;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1content_1type_1get},
			{"pjsua_msg_data_msg_body_set", "(JLorg/pjsip/pjsua/pjsua_msg_data;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1msg_1body_1set},
			{"pjsua_msg_data_msg_body_get", "(JLorg/pjsip/pjsua/pjsua_msg_data;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1msg_1data_1msg_1body_1get},
			{"new_pjsua_msg_data", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1msg_1data},
			{"delete_pjsua_msg_data", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1msg_1data},
			{"msg_data_init", "(JLorg/pjsip/pjsua/pjsua_msg_data;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_msg_1data_1init},
			{"create", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_create},
			{"init", "(JLorg/pjsip/pjsua/pjsua_config;JLorg/pjsip/pjsua/pjsua_logging_config;JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_init},
			{"start", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_start},
			{"destroy", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_destroy},
			{"handle_events", "(J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_handle_1events},
			{"pool_create", "(Ljava/lang/String;JJ)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pool_1create},
			{"reconfigure_logging", "(JLorg/pjsip/pjsua/pjsua_logging_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_reconfigure_1logging},
			{"get_pjsip_endpt", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1pjsip_1endpt},
			{"get_pjmedia_endpt", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1pjmedia_1endpt},
			{"get_pool_factory", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1pool_1factory},
			{"detect_nat_type", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_detect_1nat_1type},
			{"get_nat_type", "(J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1nat_1type},
			{"verify_sip_url", "(Ljava/lang/String;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_verify_1sip_1url},
			{"perror", "(Ljava/lang/String;Ljava/lang/String;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_perror},
			{"dump", "(I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_dump},
			{"pjsua_transport_config_port_set", "(JLorg/pjsip/pjsua/pjsua_transport_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1port_1set},
			{"pjsua_transport_config_port_get", "(JLorg/pjsip/pjsua/pjsua_transport_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1port_1get},
			{"pjsua_transport_config_public_addr_set", "(JLorg/pjsip/pjsua/pjsua_transport_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1public_1addr_1set},
			{"pjsua_transport_config_public_addr_get", "(JLorg/pjsip/pjsua/pjsua_transport_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1public_1addr_1get},
			{"pjsua_transport_config_bound_addr_set", "(JLorg/pjsip/pjsua/pjsua_transport_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1bound_1addr_1set},
			{"pjsua_transport_config_bound_addr_get", "(JLorg/pjsip/pjsua/pjsua_transport_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1bound_1addr_1get},
			{"pjsua_transport_config_tls_setting_set", "(JLorg/pjsip/pjsua/pjsua_transport_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1tls_1setting_1set},
			{"pjsua_transport_config_tls_setting_get", "(JLorg/pjsip/pjsua/pjsua_transport_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1config_1tls_1setting_1get},
			{"new_pjsua_transport_config", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1transport_1config},
			{"delete_pjsua_transport_config", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1transport_1config},
			{"transport_config_default", "(JLorg/pjsip/pjsua/pjsua_transport_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1config_1default},
			{"transport_config_dup", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pjsua_transport_config;JLorg/pjsip/pjsua/pjsua_transport_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1config_1dup},
			{"pjsua_transport_info_id_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1id_1set},
			{"pjsua_transport_info_id_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1id_1get},
			{"pjsua_transport_info_type_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1set},
			{"pjsua_transport_info_type_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1get},
			{"pjsua_transport_info_type_name_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1name_1set},
			{"pjsua_transport_info_type_name_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1type_1name_1get},
			{"pjsua_transport_info_info_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1info_1set},
			{"pjsua_transport_info_info_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1info_1get},
			{"pjsua_transport_info_flag_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1flag_1set},
			{"pjsua_transport_info_flag_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1flag_1get},
			{"pjsua_transport_info_addr_len_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1addr_1len_1set},
			{"pjsua_transport_info_addr_len_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1addr_1len_1get},
			{"pjsua_transport_info_local_addr_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1addr_1set},
			{"pjsua_transport_info_local_addr_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1addr_1get},
			{"pjsua_transport_info_local_name_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1name_1set},
			{"pjsua_transport_info_local_name_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1local_1name_1get},
			{"pjsua_transport_info_usage_count_set", "(JLorg/pjsip/pjsua/pjsua_transport_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1usage_1count_1set},
			{"pjsua_transport_info_usage_count_get", "(JLorg/pjsip/pjsua/pjsua_transport_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1transport_1info_1usage_1count_1get},
			{"new_pjsua_transport_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1transport_1info},
			{"delete_pjsua_transport_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1transport_1info},
			{"transport_create", "(IJLorg/pjsip/pjsua/pjsua_transport_config;J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1create},
			{"transport_register", "(JJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1register},
			{"transport_get_count", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1get_1count},
			{"enum_transports", "([I[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1transports},
			{"transport_get_info", "(IJLorg/pjsip/pjsua/pjsua_transport_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1get_1info},
			{"transport_set_enable", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1set_1enable},
			{"transport_close", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_transport_1close},
			{"PJSUA_MAX_ACC_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1ACC_1get},
			{"PJSUA_REG_INTERVAL_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1REG_1INTERVAL_1get},
			{"PJSUA_PUBLISH_EXPIRATION_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1PUBLISH_1EXPIRATION_1get},
			{"PJSUA_DEFAULT_ACC_PRIORITY_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1ACC_1PRIORITY_1get},
			{"PJSUA_SECURE_SCHEME_get", "()Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1SECURE_1SCHEME_1get},
			{"pjsua_acc_config_priority_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1priority_1set},
			{"pjsua_acc_config_priority_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1priority_1get},
			{"pjsua_acc_config_id_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1id_1set},
			{"pjsua_acc_config_id_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1id_1get},
			{"pjsua_acc_config_reg_uri_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1uri_1set},
			{"pjsua_acc_config_reg_uri_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1uri_1get},
			{"pjsua_acc_config_publish_enabled_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1publish_1enabled_1set},
			{"pjsua_acc_config_publish_enabled_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1publish_1enabled_1get},
			{"pjsua_acc_config_auth_pref_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1auth_1pref_1set},
			{"pjsua_acc_config_auth_pref_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1auth_1pref_1get},
			{"pjsua_acc_config_pidf_tuple_id_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1pidf_1tuple_1id_1set},
			{"pjsua_acc_config_pidf_tuple_id_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1pidf_1tuple_1id_1get},
			{"pjsua_acc_config_force_contact_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1force_1contact_1set},
			{"pjsua_acc_config_force_contact_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1force_1contact_1get},
			{"pjsua_acc_config_require_100rel_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1require_1100rel_1set},
			{"pjsua_acc_config_require_100rel_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1require_1100rel_1get},
			{"pjsua_acc_config_proxy_cnt_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1cnt_1set},
			{"pjsua_acc_config_proxy_cnt_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1cnt_1get},
			{"pjsua_acc_config_proxy_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1set},
			{"pjsua_acc_config_proxy_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1proxy_1get},
			{"pjsua_acc_config_reg_timeout_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1timeout_1set},
			{"pjsua_acc_config_reg_timeout_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1reg_1timeout_1get},
			{"pjsua_acc_config_cred_count_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1count_1set},
			{"pjsua_acc_config_cred_count_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1count_1get},
			{"pjsua_acc_config_cred_info_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pjsip_cred_info;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1info_1set},
			{"pjsua_acc_config_cred_info_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1cred_1info_1get},
			{"pjsua_acc_config_transport_id_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1transport_1id_1set},
			{"pjsua_acc_config_transport_id_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1transport_1id_1get},
			{"pjsua_acc_config_allow_contact_rewrite_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1allow_1contact_1rewrite_1set},
			{"pjsua_acc_config_allow_contact_rewrite_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1allow_1contact_1rewrite_1get},
			{"pjsua_acc_config_ka_interval_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1interval_1set},
			{"pjsua_acc_config_ka_interval_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1interval_1get},
			{"pjsua_acc_config_ka_data_set", "(JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1data_1set},
			{"pjsua_acc_config_ka_data_get", "(JLorg/pjsip/pjsua/pjsua_acc_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1config_1ka_1data_1get},
			{"new_pjsua_acc_config", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1acc_1config},
			{"delete_pjsua_acc_config", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1acc_1config},
			{"acc_config_default", "(JLorg/pjsip/pjsua/pjsua_acc_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1config_1default},
			{"acc_config_dup", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pjsua_acc_config;JLorg/pjsip/pjsua/pjsua_acc_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1config_1dup},
			{"pjsua_acc_info_id_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1id_1set},
			{"pjsua_acc_info_id_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1id_1get},
			{"pjsua_acc_info_is_default_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1is_1default_1set},
			{"pjsua_acc_info_is_default_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1is_1default_1get},
			{"pjsua_acc_info_acc_uri_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1acc_1uri_1set},
			{"pjsua_acc_info_acc_uri_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1acc_1uri_1get},
			{"pjsua_acc_info_has_registration_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1has_1registration_1set},
			{"pjsua_acc_info_has_registration_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1has_1registration_1get},
			{"pjsua_acc_info_expires_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1expires_1set},
			{"pjsua_acc_info_expires_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1expires_1get},
			{"pjsua_acc_info_status_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1set},
			{"pjsua_acc_info_status_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1get},
			{"pjsua_acc_info_status_text_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1text_1set},
			{"pjsua_acc_info_status_text_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1status_1text_1get},
			{"pjsua_acc_info_online_status_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1set},
			{"pjsua_acc_info_online_status_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1get},
			{"pjsua_acc_info_online_status_text_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1text_1set},
			{"pjsua_acc_info_online_status_text_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1online_1status_1text_1get},
			{"pjsua_acc_info_rpid_set", "(JLorg/pjsip/pjsua/pjsua_acc_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1rpid_1set},
			{"pjsua_acc_info_rpid_get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1rpid_1get},
			{"pjsua_acc_info_buf__set", "(JLorg/pjsip/pjsua/pjsua_acc_info;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1buf_1_1set},
			{"pjsua_acc_info_buf__get", "(JLorg/pjsip/pjsua/pjsua_acc_info;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1acc_1info_1buf_1_1get},
			{"new_pjsua_acc_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1acc_1info},
			{"delete_pjsua_acc_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1acc_1info},
			{"acc_get_count", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1get_1count},
			{"acc_is_valid", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1is_1valid},
			{"acc_set_default", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1default},
			{"acc_get_default", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1get_1default},
			{"acc_add", "(JLorg/pjsip/pjsua/pjsua_acc_config;I[I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1add},
			{"acc_add_local", "(II[I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1add_1local},
			{"acc_del", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1del},
			{"acc_modify", "(IJLorg/pjsip/pjsua/pjsua_acc_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1modify},
			{"acc_set_online_status", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1online_1status},
			{"acc_set_online_status2", "(IIJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1online_1status2},
			{"acc_set_registration", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1registration},
			{"acc_get_info", "(IJLorg/pjsip/pjsua/pjsua_acc_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1get_1info},
			{"enum_accs", "([I[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1accs},
			{"acc_enum_info", "(JLorg/pjsip/pjsua/pjsua_acc_info;[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1enum_1info},
			{"acc_find_for_outgoing", "(JLorg/pjsip/pjsua/pj_str_t;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1find_1for_1outgoing},
			{"acc_find_for_incoming", "(J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1find_1for_1incoming},
			{"acc_create_request", "(IJJLorg/pjsip/pjsua/pj_str_t;J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1create_1request},
			{"acc_create_uac_contact", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pj_str_t;IJLorg/pjsip/pjsua/pj_str_t;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1create_1uac_1contact},
			{"acc_create_uas_contact", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pj_str_t;IJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1create_1uas_1contact},
			{"acc_set_transport", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_acc_1set_1transport},
			{"PJSUA_MAX_CALLS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1CALLS_1get},
			{"pjsua_call_info_id_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1id_1set},
			{"pjsua_call_info_id_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1id_1get},
			{"pjsua_call_info_role_set", "(JLorg/pjsip/pjsua/pjsua_call_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1role_1set},
			{"pjsua_call_info_role_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1role_1get},
			{"pjsua_call_info_acc_id_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1acc_1id_1set},
			{"pjsua_call_info_acc_id_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1acc_1id_1get},
			{"pjsua_call_info_local_info_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1info_1set},
			{"pjsua_call_info_local_info_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1info_1get},
			{"pjsua_call_info_local_contact_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1contact_1set},
			{"pjsua_call_info_local_contact_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1local_1contact_1get},
			{"pjsua_call_info_remote_info_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1info_1set},
			{"pjsua_call_info_remote_info_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1info_1get},
			{"pjsua_call_info_remote_contact_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1contact_1set},
			{"pjsua_call_info_remote_contact_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1remote_1contact_1get},
			{"pjsua_call_info_call_id_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1call_1id_1set},
			{"pjsua_call_info_call_id_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1call_1id_1get},
			{"pjsua_call_info_state_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1set},
			{"pjsua_call_info_state_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1get},
			{"pjsua_call_info_state_text_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1text_1set},
			{"pjsua_call_info_state_text_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1state_1text_1get},
			{"pjsua_call_info_last_status_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1set},
			{"pjsua_call_info_last_status_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1get},
			{"pjsua_call_info_last_status_text_set", "(JLorg/pjsip/pjsua/pjsua_call_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1text_1set},
			{"pjsua_call_info_last_status_text_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1last_1status_1text_1get},
			{"pjsua_call_info_media_status_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1status_1set},
			{"pjsua_call_info_media_status_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1status_1get},
			{"pjsua_call_info_media_dir_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1dir_1set},
			{"pjsua_call_info_media_dir_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1media_1dir_1get},
			{"pjsua_call_info_conf_slot_set", "(JLorg/pjsip/pjsua/pjsua_call_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1conf_1slot_1set},
			{"pjsua_call_info_conf_slot_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1conf_1slot_1get},
			{"pjsua_call_info_connect_duration_set", "(JLorg/pjsip/pjsua/pjsua_call_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1connect_1duration_1set},
			{"pjsua_call_info_connect_duration_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1connect_1duration_1get},
			{"pjsua_call_info_total_duration_set", "(JLorg/pjsip/pjsua/pjsua_call_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1total_1duration_1set},
			{"pjsua_call_info_total_duration_get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1total_1duration_1get},
			{"pjsua_call_info_buf__get", "(JLorg/pjsip/pjsua/pjsua_call_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1get},
			{"new_pjsua_call_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1call_1info},
			{"delete_pjsua_call_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1call_1info},
			{"pjsua_call_info_buf__local_info_set", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1info_1set},
			{"pjsua_call_info_buf__local_info_get", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1info_1get},
			{"pjsua_call_info_buf__local_contact_set", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1contact_1set},
			{"pjsua_call_info_buf__local_contact_get", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1local_1contact_1get},
			{"pjsua_call_info_buf__remote_info_set", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1info_1set},
			{"pjsua_call_info_buf__remote_info_get", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1info_1get},
			{"pjsua_call_info_buf__remote_contact_set", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1contact_1set},
			{"pjsua_call_info_buf__remote_contact_get", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1remote_1contact_1get},
			{"pjsua_call_info_buf__call_id_set", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1call_1id_1set},
			{"pjsua_call_info_buf__call_id_get", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1call_1id_1get},
			{"pjsua_call_info_buf__last_status_text_set", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1last_1status_1text_1set},
			{"pjsua_call_info_buf__last_status_text_get", "(JLorg/pjsip/pjsua/pjsua_call_info_buf_;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1call_1info_1buf_1_1last_1status_1text_1get},
			{"new_pjsua_call_info_buf_", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1call_1info_1buf_1},
			{"delete_pjsua_call_info_buf_", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1call_1info_1buf_1},
			{"call_get_max_count", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1max_1count},
			{"call_get_count", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1count},
			{"enum_calls", "([I[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1calls},
			{"call_make_call", "(IJLorg/pjsip/pjsua/pj_str_t;J[BJLorg/pjsip/pjsua/pjsua_msg_data;[I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1make_1call},
			{"call_is_active", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1is_1active},
			{"call_has_media", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1has_1media},
			{"call_get_conf_port", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1conf_1port},
			{"call_get_info", "(IJLorg/pjsip/pjsua/pjsua_call_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1info},
			{"call_set_user_data", "(I[B)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1set_1user_1data},
			{"call_get_rem_nat_type", "(IJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1get_1rem_1nat_1type},
			{"call_answer", "(IJJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1answer},
			{"call_hangup", "(IJJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1hangup},
			{"call_set_hold", "(IJLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1set_1hold},
			{"call_reinvite", "(IIJLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1reinvite},
			{"call_update", "(IJJLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1update},
			{"call_xfer", "(IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1xfer},
			{"PJSUA_XFER_NO_REQUIRE_REPLACES_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1XFER_1NO_1REQUIRE_1REPLACES_1get},
			{"call_xfer_replaces", "(IIJJLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1xfer_1replaces},
			{"call_dial_dtmf", "(IJLorg/pjsip/pjsua/pj_str_t;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1dial_1dtmf},
			{"call_send_im", "(IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjsua_msg_data;[B)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1send_1im},
			{"call_send_typing_ind", "(IIJLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1send_1typing_1ind},
			{"call_send_request", "(IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1send_1request},
			{"call_hangup_all", "()V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1hangup_1all},
			{"call_dump", "(IILjava/lang/String;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_call_1dump},
			{"PJSUA_MAX_BUDDIES_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1BUDDIES_1get},
			{"PJSUA_PRES_TIMER_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1PRES_1TIMER_1get},
			{"pjsua_buddy_config_uri_set", "(JLorg/pjsip/pjsua/pjsua_buddy_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1uri_1set},
			{"pjsua_buddy_config_uri_get", "(JLorg/pjsip/pjsua/pjsua_buddy_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1uri_1get},
			{"pjsua_buddy_config_subscribe_set", "(JLorg/pjsip/pjsua/pjsua_buddy_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1subscribe_1set},
			{"pjsua_buddy_config_subscribe_get", "(JLorg/pjsip/pjsua/pjsua_buddy_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1config_1subscribe_1get},
			{"new_pjsua_buddy_config", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1buddy_1config},
			{"delete_pjsua_buddy_config", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1buddy_1config},
			{"pjsua_buddy_info_id_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1id_1set},
			{"pjsua_buddy_info_id_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1id_1get},
			{"pjsua_buddy_info_uri_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1uri_1set},
			{"pjsua_buddy_info_uri_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1uri_1get},
			{"pjsua_buddy_info_contact_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1contact_1set},
			{"pjsua_buddy_info_contact_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1contact_1get},
			{"pjsua_buddy_info_status_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1set},
			{"pjsua_buddy_info_status_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1get},
			{"pjsua_buddy_info_status_text_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1text_1set},
			{"pjsua_buddy_info_status_text_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1status_1text_1get},
			{"pjsua_buddy_info_monitor_pres_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1monitor_1pres_1set},
			{"pjsua_buddy_info_monitor_pres_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1monitor_1pres_1get},
			{"pjsua_buddy_info_rpid_set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1rpid_1set},
			{"pjsua_buddy_info_rpid_get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1rpid_1get},
			{"pjsua_buddy_info_buf__set", "(JLorg/pjsip/pjsua/pjsua_buddy_info;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1buf_1_1set},
			{"pjsua_buddy_info_buf__get", "(JLorg/pjsip/pjsua/pjsua_buddy_info;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1buddy_1info_1buf_1_1get},
			{"new_pjsua_buddy_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1buddy_1info},
			{"delete_pjsua_buddy_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1buddy_1info},
			{"buddy_config_default", "(JLorg/pjsip/pjsua/pjsua_buddy_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1config_1default},
			{"get_buddy_count", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1buddy_1count},
			{"buddy_is_valid", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1is_1valid},
			{"enum_buddies", "([I[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1buddies},
			{"buddy_get_info", "(IJLorg/pjsip/pjsua/pjsua_buddy_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1get_1info},
			{"buddy_add", "(JLorg/pjsip/pjsua/pjsua_buddy_config;J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1add},
			{"buddy_del", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1del},
			{"buddy_subscribe_pres", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1subscribe_1pres},
			{"buddy_update_pres", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_buddy_1update_1pres},
			{"pres_dump", "(I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pres_1dump},
			{"pjsip_message_method_get", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsip_1message_1method_1get},
			{"im_send", "(IJLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjsua_msg_data;[B)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_im_1send},
			{"im_typing", "(IJLorg/pjsip/pjsua/pj_str_t;IJLorg/pjsip/pjsua/pjsua_msg_data;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_im_1typing},
			{"PJSUA_MAX_CONF_PORTS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1CONF_1PORTS_1get},
			{"PJSUA_DEFAULT_CLOCK_RATE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1CLOCK_1RATE_1get},
			{"PJSUA_DEFAULT_AUDIO_FRAME_PTIME_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1AUDIO_1FRAME_1PTIME_1get},
			{"PJSUA_DEFAULT_CODEC_QUALITY_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1CODEC_1QUALITY_1get},
			{"PJSUA_DEFAULT_ILBC_MODE_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1ILBC_1MODE_1get},
			{"PJSUA_DEFAULT_EC_TAIL_LEN_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1DEFAULT_1EC_1TAIL_1LEN_1get},
			{"PJSUA_MAX_PLAYERS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1PLAYERS_1get},
			{"PJSUA_MAX_RECORDERS_get", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_PJSUA_1MAX_1RECORDERS_1get},
			{"pjsua_media_config_clock_rate_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1clock_1rate_1set},
			{"pjsua_media_config_clock_rate_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1clock_1rate_1get},
			{"pjsua_media_config_snd_clock_rate_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1clock_1rate_1set},
			{"pjsua_media_config_snd_clock_rate_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1clock_1rate_1get},
			{"pjsua_media_config_channel_count_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1channel_1count_1set},
			{"pjsua_media_config_channel_count_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1channel_1count_1get},
			{"pjsua_media_config_audio_frame_ptime_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1audio_1frame_1ptime_1set},
			{"pjsua_media_config_audio_frame_ptime_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1audio_1frame_1ptime_1get},
			{"pjsua_media_config_max_media_ports_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1max_1media_1ports_1set},
			{"pjsua_media_config_max_media_ports_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1max_1media_1ports_1get},
			{"pjsua_media_config_has_ioqueue_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1has_1ioqueue_1set},
			{"pjsua_media_config_has_ioqueue_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1has_1ioqueue_1get},
			{"pjsua_media_config_thread_cnt_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1thread_1cnt_1set},
			{"pjsua_media_config_thread_cnt_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1thread_1cnt_1get},
			{"pjsua_media_config_quality_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1quality_1set},
			{"pjsua_media_config_quality_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1quality_1get},
			{"pjsua_media_config_ptime_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ptime_1set},
			{"pjsua_media_config_ptime_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ptime_1get},
			{"pjsua_media_config_no_vad_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1no_1vad_1set},
			{"pjsua_media_config_no_vad_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1no_1vad_1get},
			{"pjsua_media_config_ilbc_mode_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ilbc_1mode_1set},
			{"pjsua_media_config_ilbc_mode_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ilbc_1mode_1get},
			{"pjsua_media_config_tx_drop_pct_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1tx_1drop_1pct_1set},
			{"pjsua_media_config_tx_drop_pct_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1tx_1drop_1pct_1get},
			{"pjsua_media_config_rx_drop_pct_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1rx_1drop_1pct_1set},
			{"pjsua_media_config_rx_drop_pct_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1rx_1drop_1pct_1get},
			{"pjsua_media_config_ec_options_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1options_1set},
			{"pjsua_media_config_ec_options_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1options_1get},
			{"pjsua_media_config_ec_tail_len_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1tail_1len_1set},
			{"pjsua_media_config_ec_tail_len_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ec_1tail_1len_1get},
			{"pjsua_media_config_jb_init_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1init_1set},
			{"pjsua_media_config_jb_init_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1init_1get},
			{"pjsua_media_config_jb_min_pre_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1min_1pre_1set},
			{"pjsua_media_config_jb_min_pre_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1min_1pre_1get},
			{"pjsua_media_config_jb_max_pre_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1pre_1set},
			{"pjsua_media_config_jb_max_pre_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1pre_1get},
			{"pjsua_media_config_jb_max_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1set},
			{"pjsua_media_config_jb_max_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1jb_1max_1get},
			{"pjsua_media_config_enable_ice_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1ice_1set},
			{"pjsua_media_config_enable_ice_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1ice_1get},
			{"pjsua_media_config_ice_no_host_cands_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ice_1no_1host_1cands_1set},
			{"pjsua_media_config_ice_no_host_cands_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1ice_1no_1host_1cands_1get},
			{"pjsua_media_config_enable_turn_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1turn_1set},
			{"pjsua_media_config_enable_turn_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1enable_1turn_1get},
			{"pjsua_media_config_turn_server_set", "(JLorg/pjsip/pjsua/pjsua_media_config;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1server_1set},
			{"pjsua_media_config_turn_server_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1server_1get},
			{"pjsua_media_config_turn_conn_type_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1conn_1type_1set},
			{"pjsua_media_config_turn_conn_type_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1conn_1type_1get},
			{"pjsua_media_config_turn_auth_cred_set", "(JLorg/pjsip/pjsua/pjsua_media_config;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1auth_1cred_1set},
			{"pjsua_media_config_turn_auth_cred_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1turn_1auth_1cred_1get},
			{"pjsua_media_config_snd_auto_close_time_set", "(JLorg/pjsip/pjsua/pjsua_media_config;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1auto_1close_1time_1set},
			{"pjsua_media_config_snd_auto_close_time_get", "(JLorg/pjsip/pjsua/pjsua_media_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1config_1snd_1auto_1close_1time_1get},
			{"new_pjsua_media_config", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1media_1config},
			{"delete_pjsua_media_config", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1media_1config},
			{"media_config_default", "(JLorg/pjsip/pjsua/pjsua_media_config;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_media_1config_1default},
			{"pjsua_codec_info_codec_id_set", "(JLorg/pjsip/pjsua/pjsua_codec_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1codec_1id_1set},
			{"pjsua_codec_info_codec_id_get", "(JLorg/pjsip/pjsua/pjsua_codec_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1codec_1id_1get},
			{"pjsua_codec_info_priority_set", "(JLorg/pjsip/pjsua/pjsua_codec_info;S)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1priority_1set},
			{"pjsua_codec_info_priority_get", "(JLorg/pjsip/pjsua/pjsua_codec_info;)S", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1priority_1get},
			{"pjsua_codec_info_buf__set", "(JLorg/pjsip/pjsua/pjsua_codec_info;Ljava/lang/String;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1buf_1_1set},
			{"pjsua_codec_info_buf__get", "(JLorg/pjsip/pjsua/pjsua_codec_info;)Ljava/lang/String;", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1codec_1info_1buf_1_1get},
			{"new_pjsua_codec_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1codec_1info},
			{"delete_pjsua_codec_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1codec_1info},
			{"pjsua_conf_port_info_slot_id_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1slot_1id_1set},
			{"pjsua_conf_port_info_slot_id_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1slot_1id_1get},
			{"pjsua_conf_port_info_name_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;JLorg/pjsip/pjsua/pj_str_t;)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1name_1set},
			{"pjsua_conf_port_info_name_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1name_1get},
			{"pjsua_conf_port_info_clock_rate_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1clock_1rate_1set},
			{"pjsua_conf_port_info_clock_rate_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1clock_1rate_1get},
			{"pjsua_conf_port_info_channel_count_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1channel_1count_1set},
			{"pjsua_conf_port_info_channel_count_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1channel_1count_1get},
			{"pjsua_conf_port_info_samples_per_frame_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1samples_1per_1frame_1set},
			{"pjsua_conf_port_info_samples_per_frame_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1samples_1per_1frame_1get},
			{"pjsua_conf_port_info_bits_per_sample_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1bits_1per_1sample_1set},
			{"pjsua_conf_port_info_bits_per_sample_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1bits_1per_1sample_1get},
			{"pjsua_conf_port_info_listener_cnt_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listener_1cnt_1set},
			{"pjsua_conf_port_info_listener_cnt_get", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listener_1cnt_1get},
			{"pjsua_conf_port_info_listeners_set", "(JLorg/pjsip/pjsua/pjsua_conf_port_info;[I)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1conf_1port_1info_1listeners_1set},
			{"new_pjsua_conf_port_info", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1conf_1port_1info},
			{"delete_pjsua_conf_port_info", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1conf_1port_1info},
			{"pjsua_media_transport_skinfo_set", "(JLorg/pjsip/pjsua/pjsua_media_transport;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1skinfo_1set},
			{"pjsua_media_transport_skinfo_get", "(JLorg/pjsip/pjsua/pjsua_media_transport;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1skinfo_1get},
			{"pjsua_media_transport_transport_set", "(JLorg/pjsip/pjsua/pjsua_media_transport;J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1transport_1set},
			{"pjsua_media_transport_transport_get", "(JLorg/pjsip/pjsua/pjsua_media_transport;)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_pjsua_1media_1transport_1transport_1get},
			{"new_pjsua_media_transport", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_new_1pjsua_1media_1transport},
			{"delete_pjsua_media_transport", "(J)V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_delete_1pjsua_1media_1transport},
			{"conf_get_max_ports", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1max_1ports},
			{"conf_get_active_ports", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1active_1ports},
			{"enum_conf_ports", "([I[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1conf_1ports},
			{"conf_get_port_info", "(IJLorg/pjsip/pjsua/pjsua_conf_port_info;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1port_1info},
			{"conf_add_port", "(JLorg/pjsip/pjsua/pj_pool_t;JLorg/pjsip/pjsua/pjmedia_port;[I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1add_1port},
			{"conf_remove_port", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1remove_1port},
			{"conf_connect", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1connect},
			{"conf_disconnect", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1disconnect},
			{"conf_adjust_tx_level", "(IF)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1adjust_1tx_1level},
			{"conf_adjust_rx_level", "(IF)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1adjust_1rx_1level},
			{"conf_get_signal_level", "(IJJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_conf_1get_1signal_1level},
			{"player_create", "(JLorg/pjsip/pjsua/pj_str_t;JJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_player_1create},
			{"playlist_create", "(JLorg/pjsip/pjsua/pj_str_t;JJLorg/pjsip/pjsua/pj_str_t;JJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_playlist_1create},
			{"player_get_conf_port", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_player_1get_1conf_1port},
			{"player_get_port", "(IJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_player_1get_1port},
			{"player_set_pos", "(IJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_player_1set_1pos},
			{"player_destroy", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_player_1destroy},
			{"recorder_create", "(JLorg/pjsip/pjsua/pj_str_t;J[BJJJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_recorder_1create},
			{"recorder_get_conf_port", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_recorder_1get_1conf_1port},
			{"recorder_get_port", "(IJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_recorder_1get_1port},
			{"recorder_destroy", "(I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_recorder_1destroy},
			{"enum_snd_devs", "(JLorg/pjsip/pjsua/pjmedia_snd_dev_info;[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1snd_1devs},
			{"get_snd_dev", "([I[I)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1snd_1dev},
			{"set_snd_dev", "(II)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_set_1snd_1dev},
			{"set_null_snd_dev", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_set_1null_1snd_1dev},
			{"set_no_snd_dev", "()J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_set_1no_1snd_1dev},
			{"set_ec", "(JJ)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_set_1ec},
			{"get_ec_tail", "(J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1ec_1tail},
			{"enum_codecs", "(JLorg/pjsip/pjsua/pjsua_codec_info;[J)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_enum_1codecs},
			{"get_nbr_of_codecs", "()I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_get_1nbr_1of_1codecs},
			{"codec_get_id", "(I)J", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_codec_1get_1id},
			{"codec_set_priority", "(JLorg/pjsip/pjsua/pj_str_t;S)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_codec_1set_1priority},
			{"codec_get_param", "(JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjmedia_codec_param;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_codec_1get_1param},
			{"codec_set_param", "(JLorg/pjsip/pjsua/pj_str_t;JLorg/pjsip/pjsua/pjmedia_codec_param;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_codec_1set_1param},
			{"media_transports_create", "(JLorg/pjsip/pjsua/pjsua_transport_config;)I", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_media_1transports_1create},
			{"swig_module_init", "()V", (void*)& Java_org_pjsip_pjsua_pjsuaJNI_swig_1module_1init}
	};

	r = env->RegisterNatives (k, methods, NELEM(methods));
	return JNI_VERSION_1_4;
}



#ifdef __cplusplus
}
#endif
