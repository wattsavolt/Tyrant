#include "GraphicsUtility.h"

namespace tyr
{
    float GraphicsUtility::CalculateAspectRatio(const ViewArea& viewArea, uint targetWidth, uint targetHeight)
    {
        const float viewWidthPixels = viewArea.width * static_cast<float>(targetWidth);
        const float viewHeightPixels = viewArea.height * static_cast<float>(targetHeight);
        return viewWidthPixels / viewHeightPixels;
    }

    void GraphicsUtility::CreateRenderArea(const ViewArea& viewArea, uint targetWidth, uint targetHeight, GraphicsRect& renderArea)
    {
        renderArea.offset =
        {
            static_cast<int>(viewArea.x * static_cast<float>(targetWidth)),
            static_cast<int>(viewArea.y * static_cast<float>(targetHeight))
        };
        renderArea.extents =
        {
            static_cast<uint>(viewArea.width * static_cast<float>(targetWidth)),
            static_cast<uint>(viewArea.height * static_cast<float>(targetHeight))
        };
    }
}