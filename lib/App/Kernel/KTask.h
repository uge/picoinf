#include "FreeRTOS.h"
#include "task.h"

#include <string>
#include <functional>
using namespace std;


class KTask
{
public:
    KTask(string name, function<void()> fn)
    : name_(name)
    , fn_(fn)
    {
        BaseType_t status = xTaskCreate(TaskRunner, name_.c_str(),  128, this, 1, &h_);
    }

private:
    static void TaskRunner(void* pKtask)
    {
        KTask *p = (KTask *)pKtask;

        p->fn_();
    }

private:
    string name_;
    function<void()> fn_;
    TaskHandle_t h_;
};
