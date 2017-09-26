//The MIT License (MIT)
//
//Copyright (c) 2017 Stefano Cristiano
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

// ---------------------------------------------------------------------------
// Box Shadow for imgui using vertex colors
// Current Limitations:
// - Only works for axis aligned non rounded rectangle
// - Not optimized at all
// - User needs to figure out how many slices / rings are needed and what 
//   spacing would look good for a box of given size and shadow sigma.
//	 Ideally the parameters should be automatically calculated based on some
//   high, medium, low quality settings
// ---------------------------------------------------------------------------
#ifndef DISABLE_IMGUI_EXAMPLE_INCLUDES
#include "../../imgui.h"
#include "math.h"

ImVec2 operator+(ImVec2 p1, ImVec2 p2){ return ImVec2(p1.x + p2.x, p1.y + p2.y);}
ImVec2 operator-(ImVec2 p1, ImVec2 p2){ return ImVec2(p1.x - p2.x, p1.y - p2.y);}
ImVec2 operator/(ImVec2 p1, ImVec2 p2){ return ImVec2(p1.x / p2.x, p1.y / p2.y);}
ImVec2 operator*(ImVec2 p1, int value){ return ImVec2(p1.x*value, p1.y*value);}
ImVec2 operator*(ImVec2 p1, float value){ return ImVec2(p1.x*value, p1.y*value);}
ImVec4 operator+(float val, ImVec4 p2){ return ImVec4(val + p2.x, val + p2.y, val + p2.z, val + p2.w);}
ImVec4 operator*(float val, ImVec4 p2){ return ImVec4(val * p2.x, val * p2.y, val * p2.z, val * p2.w);}
ImVec4 operator*(ImVec4 p2, float val){ return ImVec4(val * p2.x, val * p2.y, val * p2.z, val * p2.w);}
ImVec4 operator-(ImVec4 p1, ImVec4 p2){ return ImVec4(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z, p1.w - p2.w);}
ImVec4 operator*(ImVec4 p1, ImVec4 p2){ return ImVec4(p1.x * p2.x, p1.y * p2.y, p1.z * p2.z, p1.w * p2.w);}
ImVec4 operator/(ImVec4 p1, ImVec4 p2){ return ImVec4(p1.x / p2.x, p1.y / p2.y, p1.z / p2.z, p1.w / p2.w);}
#endif


ImVec4 boxGaussianIntegral(ImVec4 x)
{
    const ImVec4 s          = ImVec4(x.x > 0 ? 1.0f : -1.0f, x.y > 0 ? 1.0f : -1.0f, x.z > 0 ? 1.0f : -1.0f, x.w > 0 ? 1.0f : -1.0f);
    const ImVec4 a          = ImVec4(fabsf(x.x), fabsf(x.y), fabsf(x.z), fabsf(x.w));
    const ImVec4 res        = 1.0f + (0.278393f + (0.230389f + 0.078108f * (a * a)) * a) * a;
    const ImVec4 resSquared = res * res;
    return s - s / (resSquared * resSquared);
}

ImVec4 boxLinearInterpolation(ImVec4 x)
{
    const float maxClamp = 1.0f;
    const float minClamp = -1.0f;
    return ImVec4(x.x > maxClamp ? maxClamp : x.x < minClamp ? minClamp : x.x,
                  x.y > maxClamp ? maxClamp : x.y < minClamp ? minClamp : x.y,
                  x.z > maxClamp ? maxClamp : x.z < minClamp ? minClamp : x.z,
                  x.w > maxClamp ? maxClamp : x.w < minClamp ? minClamp : x.w);
}

float boxShadow(ImVec2 lower, ImVec2 upper, ImVec2 point, float sigma, bool linearInterpolation)
{
    const ImVec2 pointLower    = point - lower;
    const ImVec2 pointUpper    = point - upper;
    const ImVec4 query         = ImVec4(pointLower.x, pointLower.y, pointUpper.x, pointUpper.y);
    const ImVec4 pointToSample = query * (sqrtf(0.5f) / sigma);
    const ImVec4 integral      = linearInterpolation ? 0.5f + 0.5f * boxLinearInterpolation(pointToSample) : 0.5f + 0.5f * boxGaussianIntegral(pointToSample);
    return (integral.z - integral.x) * (integral.w - integral.y);
}

struct RectangleShadowSettings
{
    // Inputs
    bool    linear       = false;
    float   sigma        = 3;
    ImVec2  padding      = ImVec2(50, 50);
    ImVec2  rectPos      = ImVec2(50, 50);
    ImVec2  rectSize     = ImVec2(120, 120);
    ImVec2  shadowOffset = ImVec2(0, 0);
    ImVec2  shadowSize   = ImVec2(120, 50);
    ImColor shadowColor  = ImColor(0.6f, 0.6f, 0.6f, 1.0f);
    
    int  rings                    = 3;
    int  spacingBetweenRings      = 6;
    int  samplesPerCornerSide     = 1;
    int  spacingBetweenSamples    = 15;
    
    // Outputs
    int totalVertices = 0;
    int totalIndices  = 0;
    
    // Visualization
    bool enableDebugVisualization = false;

};

void drawRectangleShadowVerticesAdaptive(RectangleShadowSettings& settings)
{
    const int    samplesSpan            = settings.samplesPerCornerSide * settings.spacingBetweenSamples;
    const int    halfWidth              = static_cast<int>(settings.rectSize.x / 2);
    const int    numSamplesInHalfWidth  = (halfWidth / settings.spacingBetweenSamples) == 0 ? 1 : halfWidth / settings.spacingBetweenSamples;
    const int    numSamplesWidth        = samplesSpan > halfWidth ? numSamplesInHalfWidth : settings.samplesPerCornerSide;
	const int    halfHeight				= static_cast<int>(settings.rectSize.y / 2);
    const int    numSamplesInHalfHeight = (halfHeight / settings.spacingBetweenSamples) == 0 ? 1 : halfHeight / settings.spacingBetweenSamples;
    const int    numSamplesHeight       = samplesSpan > halfHeight ? numSamplesInHalfHeight : settings.samplesPerCornerSide;
    const int    numVerticesInARing     = numSamplesWidth * 4 + numSamplesHeight * 4 + 4;
    const ImVec2 whiteTexelUV           = ImGui::GetIO().Fonts->TexUvWhitePixel;
    const ImVec2 origin                 = ImGui::GetCursorScreenPos();
    const ImVec2 rectangleTopLeft       = origin + settings.rectPos;
    const ImVec2 rectangleBottomRight   = rectangleTopLeft + settings.rectSize;
    const ImVec2 rectangleTopRight      = rectangleTopLeft + ImVec2(settings.rectSize.x, 0);
    const ImVec2 rectangleBottomLeft    = rectangleTopLeft + ImVec2(0, settings.rectSize.y);
    
    ImColor shadowColor    = settings.shadowColor;
    settings.totalVertices = numVerticesInARing * settings.rings;
    settings.totalIndices  = 6 * (numVerticesInARing) * (settings.rings - 1);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PrimReserve(settings.totalIndices, settings.totalVertices);
    const ImDrawVert* shadowVertices = drawList->_VtxWritePtr;
    ImDrawVert*       vertexPointer  = drawList->_VtxWritePtr;
    
    for (int r = 0; r < settings.rings; ++r)
    {
        const float  adaptiveScale = (r / 2.5f) + 1;
        const ImVec2 ringOffset    = ImVec2(adaptiveScale * r * settings.spacingBetweenRings, adaptiveScale * r * settings.spacingBetweenRings);
        for (int j = 0; j < 4; ++j)
        {
            ImVec2      corner;
            ImVec2      direction[2];
            const float spacingBetweenSamplesOnARing = static_cast<float>(settings.spacingBetweenSamples);
            switch (j)
            {
                case 0:
                    corner       = rectangleTopLeft + ImVec2(-ringOffset.x, -ringOffset.y);
                    direction[0] = ImVec2(1, 0) * spacingBetweenSamplesOnARing;
                    direction[1] = ImVec2(0, 1) * spacingBetweenSamplesOnARing;
                    for (int i = 0; i < numSamplesWidth; ++i)
                    {
                        const ImVec2 point  = corner + direction[0] * (numSamplesWidth - i);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    
                    shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
                    vertexPointer->pos  = corner;
                    vertexPointer->uv   = whiteTexelUV;
                    vertexPointer->col  = shadowColor;
                    vertexPointer++;
                    
                    for (int i = 0; i < numSamplesHeight; ++i)
                    {
                        const ImVec2 point  = corner + direction[1] * (i + 1);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    break;
                case 1:
                    corner       = rectangleBottomLeft + ImVec2(-ringOffset.x, +ringOffset.y);
                    direction[0] = ImVec2(1, 0) * spacingBetweenSamplesOnARing;
                    direction[1] = ImVec2(0, -1) * spacingBetweenSamplesOnARing;
                    for (int i = 0; i < numSamplesHeight; ++i)
                    {
                        const ImVec2 point  = corner + direction[1] * (numSamplesHeight - i);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    
                    shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
                    vertexPointer->pos  = corner;
                    vertexPointer->uv   = whiteTexelUV;
                    vertexPointer->col  = shadowColor;
                    vertexPointer++;
                    
                    for (int i = 0; i < numSamplesWidth; ++i)
                    {
                        const ImVec2 point  = corner + direction[0] * (i + 1);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    break;
                case 2:
                    corner       = rectangleBottomRight + ImVec2(+ringOffset.x, +ringOffset.y);
                    direction[0] = ImVec2(-1, 0) * spacingBetweenSamplesOnARing;
                    direction[1] = ImVec2(0, -1) * spacingBetweenSamplesOnARing;
                    for (int i = 0; i < numSamplesWidth; ++i)
                    {
                        const ImVec2 point  = corner + direction[0] * (numSamplesWidth - i);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    
                    shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
                    vertexPointer->pos  = corner;
                    vertexPointer->uv   = whiteTexelUV;
                    vertexPointer->col  = shadowColor;
                    vertexPointer++;
                    
                    for (int i = 0; i < numSamplesHeight; ++i)
                    {
                        const ImVec2 point  = corner + direction[1] * (i + 1);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    break;
                case 3:
                    corner       = rectangleTopRight + ImVec2(+ringOffset.x, -ringOffset.y);
                    direction[0] = ImVec2(-1, 0) * spacingBetweenSamplesOnARing;
                    direction[1] = ImVec2(0, 1) * spacingBetweenSamplesOnARing;
                    for (int i = 0; i < numSamplesHeight; ++i)
                    {
                        const ImVec2 point  = corner + direction[1] * (numSamplesHeight - i);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    
                    shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, corner - settings.shadowOffset, settings.sigma, settings.linear);
                    vertexPointer->pos  = corner;
                    vertexPointer->uv   = whiteTexelUV;
                    vertexPointer->col  = shadowColor;
                    vertexPointer++;
                    
                    for (int i = 0; i < numSamplesWidth; ++i)
                    {
                        const ImVec2 point  = corner + direction[0] * (i + 1);
                        shadowColor.Value.w = boxShadow(rectangleTopLeft, rectangleBottomRight, point - settings.shadowOffset, settings.sigma, settings.linear);
                        vertexPointer->pos  = point;
                        vertexPointer->uv   = whiteTexelUV;
                        vertexPointer->col  = shadowColor;
                        vertexPointer++;
                    }
                    break;
            }
        }
    }
    
    ImDrawIdx idx = (ImDrawIdx)drawList->_VtxCurrentIdx;
    
    for (int r = 0; r < settings.rings - 1; ++r)
    {
        const ImDrawIdx startOfRingIndex = idx;
        for (int i = 0; i < numVerticesInARing - 1; ++i)
        {
            drawList->_IdxWritePtr[0] = idx + 0;
            drawList->_IdxWritePtr[1] = idx + 1;
            drawList->_IdxWritePtr[2] = idx + numVerticesInARing;
            drawList->_IdxWritePtr[3] = idx + 1;
            drawList->_IdxWritePtr[4] = idx + numVerticesInARing + 1;
            drawList->_IdxWritePtr[5] = idx + numVerticesInARing;
            
            idx += 1;
            drawList->_IdxWritePtr += 6;
        }
        
        drawList->_IdxWritePtr[0] = idx + 0;
        drawList->_IdxWritePtr[1] = startOfRingIndex + 0;
        drawList->_IdxWritePtr[2] = startOfRingIndex + numVerticesInARing;
        drawList->_IdxWritePtr[3] = idx + 0;
        drawList->_IdxWritePtr[4] = startOfRingIndex + numVerticesInARing;
        drawList->_IdxWritePtr[5] = idx + numVerticesInARing;
        
        drawList->_IdxWritePtr += 6;
        idx += 1;
    }
    drawList->_VtxCurrentIdx += settings.totalVertices;
    
    if (settings.enableDebugVisualization)
    {
        const ImColor lineColor(0, 0, 255, 50);
        for (int r = 0; r < settings.rings - 1; ++r)
        {
            const ImDrawIdx startOfRingIndex = r * numVerticesInARing;
            
            ImVec2 a;
            ImVec2 b;
            for (int i = 0; i < numVerticesInARing - 1; ++i)
            {
                a = shadowVertices[startOfRingIndex + i + 1].pos;
                b = shadowVertices[startOfRingIndex + i + numVerticesInARing].pos;
                drawList->AddLine(a, b, lineColor);
                a = shadowVertices[startOfRingIndex + i + 0].pos;
                b = shadowVertices[startOfRingIndex + i + numVerticesInARing].pos;
                drawList->AddLine(a, b, lineColor);
                a = shadowVertices[startOfRingIndex + i + numVerticesInARing + 1].pos;
                b = shadowVertices[startOfRingIndex + i + numVerticesInARing].pos;
                drawList->AddLine(a, b, lineColor);
            }
            
            a = shadowVertices[startOfRingIndex + numVerticesInARing - 1].pos;
            b = shadowVertices[startOfRingIndex + numVerticesInARing].pos;
            drawList->AddLine(a, b, lineColor);
            a = shadowVertices[startOfRingIndex + numVerticesInARing + numVerticesInARing - 1].pos;
            b = shadowVertices[startOfRingIndex + numVerticesInARing].pos;
            drawList->AddLine(a, b, lineColor);
        }
        
        for (int i = 0; i < settings.totalVertices; ++i)
        {
            const ImVec2 bmin = shadowVertices[i].pos - ImVec2(2, 2);
            const ImVec2 bmax = shadowVertices[i].pos + ImVec2(2, 2);
            drawList->AddRectFilled(bmin, bmax, ImColor(255, 0, 0, 50));
        }
    }
}
RectangleShadowSettings shadowSettings;

void drawShadowTestExampleWindow()
{
    ImGui::SetNextWindowSize(ImVec2(400,800), ImGuiCond_Once);
    ImGui::Begin("Test Shadows");
    static bool showRectangles          = true;
    static bool calculateMinimumPadding = false;
    static ImColor backgroundColor(255, 255, 255, 255);
    static ImColor rectangleColor(240, 240, 240, 255);
    ImGui::Checkbox("Wireframe", &shadowSettings.enableDebugVisualization);
    ImGui::Checkbox("Draw Rectangle", &showRectangles);
    ImGui::Checkbox("Recalculate Padding", &calculateMinimumPadding);
    ImGui::Checkbox("Linear Falloff", &shadowSettings.linear);
    ImGui::SliderFloat2("Rectangle Size", &shadowSettings.rectSize.x, 10, 256);
    ImGui::SliderFloat("Shadow Sigma", &shadowSettings.sigma, 0, 50);
    ImGui::SliderFloat2("Shadow Offset", &shadowSettings.shadowOffset.x, -10, 10);
    ImGui::SliderInt("Rings number", &shadowSettings.rings, 1, 10);
    ImGui::SliderInt("Rings spacing", &shadowSettings.spacingBetweenRings, 1, 20);
    ImGui::SliderInt("Corner samples", &shadowSettings.samplesPerCornerSide, 1, 20);
    ImGui::SliderInt("Corner spacing", &shadowSettings.spacingBetweenSamples, 1, 20);
    ImGui::ColorPicker3("Shadow Color", &shadowSettings.shadowColor.Value.x, ImGuiColorEditFlags_PickerHueWheel);
    ImGui::ColorPicker3("Rectangle Color", &rectangleColor.Value.x, ImGuiColorEditFlags_PickerHueWheel);
    ImGui::ColorPicker3("Background Color", &backgroundColor.Value.x, ImGuiColorEditFlags_PickerHueWheel);
    ImGui::End();
    
    if (calculateMinimumPadding)
    {
        shadowSettings.padding = ImVec2(3 * shadowSettings.sigma, 3 * shadowSettings.sigma);
    }
    shadowSettings.rectPos    = shadowSettings.padding;
    shadowSettings.shadowSize = shadowSettings.rectSize + shadowSettings.padding * 2;
    
    ImGui::SetNextWindowPos(ImVec2(700, 0), ImGuiCond_Once);
    ImGui::Begin("Shadow Outputs");
    ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, (ImU32)backgroundColor);
    ImGui::Text("rings:%d, spacingH:%d, spacingR:%d\ntotalVertices:%d totalIndices:%d", shadowSettings.rings, shadowSettings.spacingBetweenRings, shadowSettings.spacingBetweenSamples, shadowSettings.totalVertices, shadowSettings.totalIndices);
    if (ImGui::BeginChild("Adaptive Rectangle Shadow", shadowSettings.shadowSize))
    {
        drawRectangleShadowVerticesAdaptive(shadowSettings);
        ImDrawList*  drawList = ImGui::GetWindowDrawList();
        const ImVec2 origin(ImGui::GetCursorScreenPos());
        if (showRectangles)
            drawList->AddRectFilled(origin + shadowSettings.rectPos, origin + shadowSettings.rectPos + shadowSettings.rectSize, rectangleColor);
        drawList->AddRect(origin, origin + shadowSettings.shadowSize, ImColor(255, 0, 0, 255));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::End();
}
