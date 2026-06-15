// Copyright © 2021 CCP ehf.

#include "StdAfx.h"

#if __APPLE__

#include "BlueClipboard.h"
#import <AppKit/AppKit.h>


BlueClipboard::OperationResult BlueClipboard::GetData( std::string& data ) const
{
    @autoreleasepool
    {
        auto pasteboard = [NSPasteboard generalPasteboard];

        auto supported = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:NSPasteboardTypeString]];
        if( !supported )
        {
            return CLIPBOARD_INCOMPATIBLE_FORMAT;
        }
        NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
        data.resize( [string lengthOfBytesUsingEncoding:NSUTF8StringEncoding] );
        memcpy( &data[0], [string UTF8String], data.length() );
        return CLIPBOARD_OK;
    }
}

BlueClipboard::OperationResult BlueClipboard::GetData( std::wstring& result ) const
{
    @autoreleasepool
    {
        auto pasteboard = [NSPasteboard generalPasteboard];

        auto supported = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:NSPasteboardTypeString]];
        if( !supported )
        {
            return CLIPBOARD_INCOMPATIBLE_FORMAT;
        }
        NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
        
        NSData* data = [string dataUsingEncoding:NSUTF32LittleEndianStringEncoding];
        int32_t length = int32_t( [data length] ) / sizeof( wchar_t );
        auto characters = reinterpret_cast<const wchar_t*>( [data bytes] );
        result = std::wstring( characters, characters + length );
        return CLIPBOARD_OK;
    }
}

BlueClipboard::OperationResult BlueClipboard::SetData( const std::string& data )
{
    @autoreleasepool
    {
        auto pasteboard = [NSPasteboard generalPasteboard];
        NSString* string = [[NSString alloc] initWithBytesNoCopy:(void*)data.c_str()
                                                          length:data.length()
                                                        encoding:NSUTF8StringEncoding
                                                    freeWhenDone:NO];
        [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
        auto success = [pasteboard setString:string forType:NSPasteboardTypeString];
        return success ? CLIPBOARD_OK : CLIPBOARD_FAILURE;
    }
}

BlueClipboard::OperationResult BlueClipboard::SetData( const std::wstring& data )
{
    @autoreleasepool
    {
        auto pasteboard = [NSPasteboard generalPasteboard];
        NSString* string = [[NSString alloc] initWithBytesNoCopy:(void*)data.c_str()
                                                          length:data.length() * sizeof( wchar_t )
                                                        encoding:NSUTF32LittleEndianStringEncoding
                                                    freeWhenDone:NO];
        [pasteboard declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString] owner:nil];
        auto success = [pasteboard setString:string forType:NSPasteboardTypeString];
        return success ? CLIPBOARD_OK : CLIPBOARD_FAILURE;
    }
}

#endif