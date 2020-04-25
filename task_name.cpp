#include <string>

static thread_local std::string task_name_;

void set_task_name(const std::string &name){
    task_name_ = name;
}

const std::string &get_task_name() {
    return task_name_;
}

