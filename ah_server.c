/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/21, create
 */

# include "ah_config.h"
# include "ah_server.h"

static http_svr *svr;
static nw_state *state;
static dict_t *methods;
static rpc_clt *listener;

struct state_info {
    nw_ses  *ses;
    uint64_t ses_id;
    int64_t  request_id;
};

struct request_info {
    rpc_clt *clt;
    uint32_t cmd;
};

static void strip_last_line_break(char * const p) {
    if (p[strlen(p)-1] == '\n')
        p[strlen(p)-1] = '\0';
}

static void reply_error(nw_ses *ses, int64_t id, int code, const char *message, uint32_t status)
{
    json_t *error = json_object();
    json_object_set_new(error, "code", json_integer(code));
    json_object_set_new(error, "message", json_string(message));
    json_t *reply = json_object();
    json_object_set_new(reply, "error", error);
    json_object_set_new(reply, "result", json_null());
    json_object_set_new(reply, "id", json_integer(id));

    char *reply_str = json_dumps(reply, 0);
    send_http_response_simple(ses, status, reply_str, strlen(reply_str));
    free(reply_str);
    json_decref(reply);
}

static void reply_bad_request(nw_ses *ses)
{
    send_http_response_simple(ses, 400, NULL, 0);
}

// static void reply_internal_error(nw_ses *ses)
// {
//     send_http_response_simple(ses, 500, NULL, 0);
// }

static void reply_not_found(nw_ses *ses, int64_t id)
{
    reply_error(ses, id, 4, "method not found", 404);
}

static void reply_time_out(nw_ses *ses, int64_t id)
{
    reply_error(ses, id, 5, "service timeout", 504);
}

static void replay_success(nw_ses *ses, int64_t id/* , const char *message*/)
{
    json_t *result = json_object();
    json_object_set_new(result, "status", json_string("success"));
    // json_object_set_new(result, "message", json_string(message));
    json_t *reply = json_object();
    json_object_set_new(reply, "error", json_null());
    json_object_set_new(reply, "result", result);
    json_object_set_new(reply, "id", json_integer(id));

    char *reply_str = json_dumps(reply, 0);
    send_http_response_simple(ses, 200, reply_str, strlen(reply_str));
    free(reply_str);
    json_decref(reply);
}

static void replay_faild(nw_ses *ses, int64_t id, int code, const char *message)
{
    json_t *error = json_object();
    json_object_set_new(error, "code", json_integer(code));
    json_object_set_new(error, "message", json_string(message));
    json_t *reply = json_object();
    json_object_set_new(reply, "error", error);
    json_object_set_new(reply, "result", json_null());
    json_object_set_new(reply, "id", json_integer(id));

    char *reply_str = json_dumps(reply, 0);
    send_http_response_simple(ses, 200, reply_str, strlen(reply_str));
    free(reply_str);
    json_decref(reply);
}

static int unlock_wallet(const char *cmd)
{
    FILE *fp;
    char unlock_cmd[1024] = "";
    strcat(unlock_cmd, cmd);
    strcat(unlock_cmd, " wallet unlock --password ");
    strcat(unlock_cmd, settings.passwd);
    strcat(unlock_cmd, " 2>&1");
    log_trace("run cmd:\n%s", unlock_cmd);
    fp = popen(unlock_cmd, "r");
    if (fp == NULL)
    {
        return -1;
    }
    else
    {
        char result[10240] = "";
        char buffer[80] = "";
        while (fgets(buffer, sizeof(buffer), fp))
        {
            strcat(result, buffer);
        }
        strip_last_line_break(result);
        log_trace("cmd response:\n%s", result);
        if (strstr(result, "Unlocked:"))
        {
            return 0;
        }
        log_vip("%s", result);
        log_stderr("%s", result);
        return -1;
    }
    return -1;
}

static int handler_withdraw_request(nw_ses *ses, const char *val, int64_t id, json_t *params)
{
    char cmd[1024] = "";
    strcat(cmd, val);
    strcat(cmd, " transfer ");
    strcat(cmd, settings.funds_user);
    strcat(cmd, " ");
    if (json_array_size(params) != 4) {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    const char *to = json_string_value(json_array_get(params, 0));
    if (to == NULL) {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    strcat(cmd, to);

    strcat(cmd, " \"");
    const char *quanlity = json_string_value(json_array_get(params, 1));
    if (quanlity == NULL) {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    strcat(cmd, quanlity);
    strcat(cmd, "\" ");

    uint64_t memo = json_integer_value(json_array_get(params, 2));
    if (memo == 0) {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    uint64_t business_id = json_integer_value(json_array_get(params, 3));
    if (business_id == 0) {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    char number[20] = "";
    strcat(cmd, "\"");
    sprintf(number, "%ld", memo);
    strcat(cmd, number);
    sprintf(number, " %ld", business_id);
    strcat(cmd, number);
    strcat(cmd, "\"");

    strcat(cmd, " 2>&1");
    while (true)
    {
        log_trace("run cmd:\n%s", cmd);
        FILE *fp;
        fp = popen(cmd, "r");
        if (fp == NULL) {
            replay_faild(ses, id, 1, "server inner error");
            return -__LINE__;
        } else {
            char result[10240] = "";
            char buffer[80] = "";
            while (fgets(buffer, sizeof(buffer), fp)) {
                strcat(result, buffer);
            }
            strip_last_line_break(result);
            log_trace("cmd response:\n%s", result);
            if (strstr(result, "executed transaction")) {
                replay_success(ses, id);
            } else if (strstr(result, "Error 3120006: No available wallet") || strstr(result, "Error 3120003: Locked wallet")) {
                if (unlock_wallet(val) == -1) {
                    replay_faild(ses, id, 1, "server inner error");
                    return -__LINE__;
                }
                continue;
            } else if (strstr(result, "Usage:") || strstr(result, "ERROR:")) {
                replay_faild(ses, id, 1, "Parameter error");
                return -__LINE__;
            } else {
                replay_faild(ses, id, 1, result);
                return -__LINE__;
            }
        }
        break;
    }
    return 0;
}

static int handler_memo_request(nw_ses *ses, const char *val, int64_t id, json_t *params, int type)
{
    char cmd[1024] = "";
    strcat(cmd, val);
    strcat(cmd, " push action ");
    strcat(cmd, settings.contract_user);
    if (type == 1)
        strcat(cmd, " insert '{\"memo\":");
    else
        strcat(cmd, " erase '{\"memo\":");
    if (json_array_size(params) != 1)
    {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    uint64_t memo = json_integer_value(json_array_get(params, 0));
    if (memo == 0)
    {
        replay_faild(ses, id, 1, "Parameter error");
        return -__LINE__;
    }
    char number[20] = "";
    sprintf(number, " %ld", memo);
    strcat(cmd, number);
    strcat(cmd, "}' -p ");
    strcat(cmd, settings.contract_user);
    strcat(cmd, " 2>&1");
    while (true) {
        log_trace("run cmd:\n%s", cmd);
        FILE *fp;
        fp = popen(cmd, "r");
        if (fp == NULL) {
            replay_faild(ses, id, 1, "server inner error");
            return -__LINE__;
        } else {
            char result[10240] = "";
            char buffer[80] = "";
            while (fgets(buffer, sizeof(buffer), fp)) {
                strcat(result, buffer);
            }
            strip_last_line_break(result);
            log_trace("cmd response:\n%s", result);
            if (strstr(result, "executed transaction")) {
                replay_success(ses, id);
            } else if (strstr(result, "Error 3120006: No available wallet") || strstr(result, "Error 3120003: Locked wallet")) {
                if (unlock_wallet(val) == -1) {
                    replay_faild(ses, id, 1, "server inner error");
                    return -__LINE__;
                }
                continue;
            } else if (strstr(result, "Usage:") || strstr(result, "ERROR:")) {
                replay_faild(ses, id, 1, "Parameter error");
                return -__LINE__;
            } else {
                replay_faild(ses, id, 1, result);
                return -__LINE__;
            }
        }
        break;
    }
    return 0;
}

static int on_http_request(nw_ses *ses, http_request_t *request)
{
    log_trace("new http request, url: %s, method: %u", request->url, request->method);
    if (request->method != HTTP_POST || !request->body) {
        reply_bad_request(ses);
        return -__LINE__;
    }

    json_t *body = json_loadb(request->body, sdslen(request->body), 0, NULL);
    if (body == NULL) {
        goto decode_error;
    }
    json_t *id = json_object_get(body, "id");
    if (!id || !json_is_integer(id)) {
        goto decode_error;
    }
    json_t *method = json_object_get(body, "method");
    if (!method || !json_is_string(method)) {
        goto decode_error;
    }
    json_t *params = json_object_get(body, "params");
    if (!params || !json_is_array(params)) {
        goto decode_error;
    }
    log_trace("from: %s body: %s", nw_sock_human_addr(&ses->peer_addr), request->body);

    dict_entry *entry = dict_find(methods, json_string_value(method));
    if (entry == NULL) {
        reply_not_found(ses, json_integer_value(id));
    } else {
        if (strcmp(json_string_value(method), "balance.withdraw") == 0) {
            handler_withdraw_request(ses, entry->val, json_integer_value(id), params);
        } else if (strcmp(json_string_value(method), "contract.insert_memo") == 0) {
            handler_memo_request(ses, entry->val, json_integer_value(id), params, 1);
        } else if (strcmp(json_string_value(method), "contract.erase_memo") == 0) {
            handler_memo_request(ses, entry->val, json_integer_value(id), params, 2);
        }
    }

    json_decref(body);
    return 0;

decode_error:
    if (body)
        json_decref(body);
    sds hex = hexdump(request->body, sdslen(request->body));
    log_fatal("peer: %s, decode request fail, request body: \n%s", nw_sock_human_addr(&ses->peer_addr), hex);
    sdsfree(hex);
    reply_bad_request(ses);
    return -__LINE__;
}

static uint32_t dict_hash_func(const void *key)
{
    return dict_generic_hash_function(key, strlen(key));
}

static int dict_key_compare(const void *key1, const void *key2)
{
    return strcmp(key1, key2);
}

static void *dict_key_dup(const void *key)
{
    return strdup(key);
}

static void dict_key_free(void *key)
{
    free(key);
}

static void *dict_val_dup(const void *val)
{
    // struct request_info *obj = malloc(sizeof(struct request_info));
    // memcpy(obj, val, sizeof(struct request_info));
    // return obj;
    return strdup(val);
}

static void dict_val_free(void *val)
{
    free(val);
}

static void on_state_timeout(nw_state_entry *entry)
{
    log_error("state id: %u timeout", entry->id);
    struct state_info *info = entry->data;
    if (info->ses->id == info->ses_id) {
        reply_time_out(info->ses, info->request_id);
    }
}

static void on_listener_connect(nw_ses *ses, bool result)
{
    if (result) {
        log_info("connect listener success");
    } else {
        log_info("connect listener fail");
    }
}

static void on_listener_recv_pkg(nw_ses *ses, rpc_pkg *pkg)
{
    return;
}

static void on_listener_recv_fd(nw_ses *ses, int fd)
{
    if (nw_svr_add_clt_fd(svr->raw_svr, fd) < 0) {
        log_error("nw_svr_add_clt_fd: %d fail: %s", fd, strerror(errno));
        close(fd);
    }
}

static int init_listener_clt(void)
{
    rpc_clt_cfg cfg;
    nw_addr_t addr;
    memset(&cfg, 0, sizeof(cfg));
    cfg.name = strdup("listener");
    cfg.addr_count = 1;
    cfg.addr_arr = &addr;
    if (nw_sock_cfg_parse(AH_LISTENER_BIND, &addr, &cfg.sock_type) < 0)
        return -__LINE__;
    cfg.max_pkg_size = 1024;

    rpc_clt_type type;
    memset(&type, 0, sizeof(type));
    type.on_connect  = on_listener_connect;
    type.on_recv_pkg = on_listener_recv_pkg;
    type.on_recv_fd  = on_listener_recv_fd;

    listener = rpc_clt_create(&cfg, &type);
    if (listener == NULL)
        return -__LINE__;
    if (rpc_clt_start(listener) < 0)
        return -__LINE__;

    return 0;
}

static int add_handler(char *method, char *cmd)
{
    // struct request_info info = { .clt = clt, .cmd = cmd };
    if (dict_add(methods, method, cmd) == NULL)
        return __LINE__;
    return 0;
}

static int init_methods_handler(void)
{
    ERR_RET_LN(add_handler("balance.withdraw", settings.excutor));
    ERR_RET_LN(add_handler("contract.insert_memo", settings.excutor));
    ERR_RET_LN(add_handler("contract.erase_memo", settings.excutor));

    return 0;
}

int init_server(void)
{
    dict_types dt;
    memset(&dt, 0, sizeof(dt));
    dt.hash_function = dict_hash_func;
    dt.key_compare = dict_key_compare;
    dt.key_dup = dict_key_dup;
    dt.val_dup = dict_val_dup;
    dt.key_destructor = dict_key_free;
    dt.val_destructor = dict_val_free;
    methods = dict_create(&dt, 64);
    if (methods == NULL)
        return -__LINE__;

    nw_state_type st;
    memset(&st, 0, sizeof(st));
    st.on_timeout = on_state_timeout;
    state = nw_state_create(&st, sizeof(struct state_info));
    if (state == NULL)
        return -__LINE__;

    svr = http_svr_create(&settings.svr, on_http_request);
    if (svr == NULL)
        return -__LINE__;

    ERR_RET(init_methods_handler());
    ERR_RET(init_listener_clt());

    return 0;
}

