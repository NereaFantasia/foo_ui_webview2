#import <Cocoa/Cocoa.h>

#import <SDK/foobar2000.h>
#import <helpers/dropdown_helper.h>

namespace fb2k {
    void comboSetupHistory(NSComboBox *, cfg_dropdown_history & var);
    void comboAddToHistory(NSComboBox *, cfg_dropdown_history & var);
}
