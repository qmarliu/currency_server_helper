/*
 * Description: 
 *     History: yang@haipo.me, 2017/04/21, create
 */

# include "ah_config.h"

struct settings settings;

static int read_config_from_json(json_t *root)
{
    int ret;
    ret = load_cfg_process(root, "process", &settings.process);
    if (ret < 0) {
        printf("load process config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_log(root, "log", &settings.log);
    if (ret < 0) {
        printf("load log config fail: %d\n", ret);
        return -__LINE__;
    }
    ret = load_cfg_http_svr(root, "svr", &settings.svr);
    if (ret < 0) {
        printf("load svr config fail: %d\n", ret);
        return -__LINE__;
    }
    ERR_RET(read_cfg_real(root, "timeout", &settings.timeout, false, 1.0));
    ERR_RET(read_cfg_int(root, "worker_num", &settings.worker_num, false, 1));
    ERR_RET(read_cfg_str(root, "excutor", &settings.excutor, NULL));
    ERR_RET(read_cfg_str(root, "node", &settings.node, NULL));
    ERR_RET(read_cfg_str(root, "wallet_passwd", &settings.passwd, NULL));
    ERR_RET(read_cfg_str(root, "funds_user", &settings.funds_user, NULL));
    ERR_RET(read_cfg_str(root, "contract_user", &settings.contract_user, NULL));

    return 0;
}

int init_config(const char *path)
{
    json_error_t error;
    json_t *root = json_load_file(path, 0, &error);
    if (root == NULL) {
        printf("json_load_file from: %s fail: %s in line: %d\n", path, error.text, error.line);
        return -__LINE__;
    }
    if (!json_is_object(root)) {
        json_decref(root);
        return -__LINE__;
    }
    int ret = read_config_from_json(root);
    if (ret < 0) {
        json_decref(root);
        return ret;
    }
    json_decref(root);

    return 0;
}

