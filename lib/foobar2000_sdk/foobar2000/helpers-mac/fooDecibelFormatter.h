#import <Foundation/Foundation.h>

// This class, fooDecibelFormatter, can be considered a part of foobar2000 ABI
// It exists in all foobar2000 versions that support loading components and will never be removed or altered in incompatible manner
// Therefore there's no need to include it in components, core implementation can be safely used


@interface fooDecibelFormatter : NSFormatter

@property (nonatomic) NSNumber * minValue;
@property (nonatomic) NSNumber * maxValue;
@property (nonatomic) BOOL showSignAlways;
@end
