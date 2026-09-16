#pragma once
#include "Commandlets/Commandlet.h"
#include "SHCreateMagicCircleCommandlet.generated.h"

UCLASS()
class USHCreateMagicCircleCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    USHCreateMagicCircleCommandlet();
    virtual int32 Main(const FString& Params) override;
};
