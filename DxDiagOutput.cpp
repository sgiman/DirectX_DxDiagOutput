//*************************************************************************************
// File: DxDiagOutput.cpp (x86/x64) - win console
//
// Описание: Пример приложения для чтения информации из dxdiagn.dll по перечислению
//
// Copyright (c) Microsoft Corp. All rights reserved.
//-------------------------------------------------------------------------------------
// Visual Studio 2010
// Modified by sgiman @ 2022, jul
//*************************************************************************************
#define INITGUID
#include <windows.h>
#pragma warning( disable : 4996 ) // отключить устаревшее предупреждение
#include <strsafe.h>
#pragma warning( default : 4996 )
#include <stdio.h>
#include <assert.h>
#include <initguid.h>
#include <dxdiag.h>

//-----------------------------------------------------------------------------
// Определения и константы
//-----------------------------------------------------------------------------
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

//-----------------------------------------------------------------------------
// Функции-прототипы
//-----------------------------------------------------------------------------
HRESULT PrintContainerAndChildren( WCHAR* wszParentName, IDxDiagContainer* pDxDiagContainer );

//******************************************************************************
// Name: main()
// Описание: Точка входа для приложения. Мы используем только консольное окно
//******************************************************************************
int main( int argc, char* argv[] )
{
    HRESULT hr;

    CoInitialize( NULL );

    IDxDiagProvider* pDxDiagProvider = NULL;
    IDxDiagContainer* pDxDiagRoot = NULL;

    // Совместное создание IDxDiagProvider*
    hr = CoCreateInstance( CLSID_DxDiagProvider,
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           IID_IDxDiagProvider,
                           ( LPVOID* )&pDxDiagProvider );
    if( SUCCEEDED( hr ) ) // если FAILED(hr) то DirectX 9 не установлен
    {
	// Заполнить структуру DXDIAG_INIT_PARAMS и передать ее в IDxDiagContainer::Initialize
        // Передача TRUE для bAllowWHQLCecks позволяет dxdiag проверять наличие драйверов.
        // цифровая подпись с логотипом WHQL, который может подключаться через Интернет для обновления
        // WHQL-сертификаты.
        DXDIAG_INIT_PARAMS dxDiagInitParam;
        ZeroMemory( &dxDiagInitParam, sizeof( DXDIAG_INIT_PARAMS ) );

        dxDiagInitParam.dwSize = sizeof( DXDIAG_INIT_PARAMS );
        dxDiagInitParam.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
        dxDiagInitParam.bAllowWHQLChecks = TRUE;
        dxDiagInitParam.pReserved = NULL;

        hr = pDxDiagProvider->Initialize( &dxDiagInitParam );
        if( FAILED( hr ) )
            goto LCleanup;

        hr = pDxDiagProvider->GetRootContainer( &pDxDiagRoot );
        if( FAILED( hr ) )
            goto LCleanup;

	// Эта функция будет рекурсивно печатать свойства 
	// корневого узла и всех его дочерних элементов.
        hr = PrintContainerAndChildren( NULL, pDxDiagRoot );
        if( FAILED( hr ) )
            goto LCleanup;
    }

LCleanup:
    SAFE_RELEASE( pDxDiagRoot );
    SAFE_RELEASE( pDxDiagProvider );

    CoUninitialize();

    return 0;
}


//-----------------------------------------------------------------------------
// Имя: PrintContainerAndChildren()
// Описание: Рекурсивно вывести свойства корневого узла 
//           и всех его дочерних элементов в окно консоли
//-----------------------------------------------------------------------------
HRESULT PrintContainerAndChildren( WCHAR* wszParentName, IDxDiagContainer* pDxDiagContainer )
{
    HRESULT hr;

    DWORD dwPropCount;
    DWORD dwPropIndex;
    WCHAR wszPropName[256];
    VARIANT var;
    WCHAR wszPropValue[256];

    DWORD dwChildCount;
    DWORD dwChildIndex;
    WCHAR wszChildName[256];
    IDxDiagContainer* pChildContainer = NULL;

    VariantInit( &var );

    hr = pDxDiagContainer->GetNumberOfProps( &dwPropCount );
    if( SUCCEEDED( hr ) )
    {
        // Распечатать каждое свойство в этом контейнере
        for( dwPropIndex = 0; dwPropIndex < dwPropCount; dwPropIndex++ )
        {
            hr = pDxDiagContainer->EnumPropNames( dwPropIndex, wszPropName, 256 );
            if( SUCCEEDED( hr ) )
            {
                hr = pDxDiagContainer->GetProp( wszPropName, &var );
                if( SUCCEEDED( hr ) )
                {
                    // Выбор типа. Есть 4 разных типа:
                    switch( var.vt )
                    {
                        case VT_UI4:
                            StringCchPrintfW( wszPropValue, 256, L"%d", var.ulVal );
                            break;
                        case VT_I4:
                            StringCchPrintfW( wszPropValue, 256, L"%d", var.lVal );
                            break;
                        case VT_BOOL:
                            StringCchCopyW( wszPropValue, 256, ( var.boolVal ) ? L"true" : L"false" );
                            break;
                        case VT_BSTR:
                            StringCchCopyW( wszPropValue, 256, var.bstrVal );
                            break;
                    }

		    // Добавить родительское имя впереди, если оно есть, чтобы
                    // было его легче читать на экране
                    if( wszParentName )
                        wprintf( L"%s.%s = %s\n", wszParentName, wszPropName, wszPropValue );
                    else
                        wprintf( L"%s = %s\n", wszPropName, wszPropValue );

                    // Очистить вариант (это необходимо для освобождения памяти BSTR)
                    VariantClear( &var );
                }
            }
        }
    }

    // Рекурсивно вызовите эту функцию для каждого из своих дочерних контейнеров.
    hr = pDxDiagContainer->GetNumberOfChildContainers( &dwChildCount );
    if( SUCCEEDED( hr ) )
    {
        for( dwChildIndex = 0; dwChildIndex < dwChildCount; dwChildIndex++ )
        {
            hr = pDxDiagContainer->EnumChildContainerNames( dwChildIndex, wszChildName, 256 );
            if( SUCCEEDED( hr ) )
            {
                hr = pDxDiagContainer->GetChildContainer( wszChildName, &pChildContainer );
                if( SUCCEEDED( hr ) )
                {
                    // wszFullChildName не нужен, но используется для вывода текста
                    WCHAR wszFullChildName[256];
                    if( wszParentName )
                        StringCchPrintfW( wszFullChildName, 256, L"%s.%s", wszParentName, wszChildName );
                    else
                        StringCchCopyW( wszFullChildName, 256, wszChildName );
                    PrintContainerAndChildren( wszFullChildName, pChildContainer );

                    SAFE_RELEASE( pChildContainer );
                }
            }
        }
    }

    return S_OK;
}
