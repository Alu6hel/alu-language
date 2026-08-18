#import <UIKit/UIKit.h>
extern "C" {
    int alu_os_get_screen_width() {
        return (int)[UIScreen mainScreen].bounds.size.width;
    }
    void alu_os_show_toast(const char* msg) {
        NSString *nmsg = [NSString stringWithUTF8String:msg];
        UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Toast" message:nmsg preferredStyle:UIAlertControllerStyleAlert];
        [[UIApplication sharedApplication].keyWindow.rootViewController presentViewController:alert animated:YES completion:nil];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [alert dismissViewControllerAnimated:YES completion:nil];
        });
    }
}
