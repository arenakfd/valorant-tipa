#import "ImGui/imgui.h"
#import "ImGui/Metal/imgui_impl_metal.h"

void 绘制矩形(ImDrawList* ImDrawList, ImVec4 位置, ImU32 颜色, float 圆角 = 0.0f, bool 瞄边 = false) {
    if (瞄边) {
        ImDrawList->AddRect(ImVec2(位置.x, 位置.y), ImVec2(位置.x + 位置.z, 位置.y + 位置.w), 颜色, 圆角);
    } else {
        ImDrawList->AddRectFilled(ImVec2(位置.x, 位置.y), ImVec2(位置.x + 位置.z, 位置.y + 位置.w), 颜色, 圆角);
    };
};

void 绘制圆形(ImDrawList* ImDrawList, float 半径, ImVec2 位置, ImU32 颜色, bool 瞄边 = false) {
    if (瞄边) {
        ImDrawList->AddCircle(位置, 半径, 颜色);
    } else {
        ImDrawList->AddCircleFilled(位置, 半径, 颜色);
    };
};

void 绘制文字(ImDrawList* ImDrawList, string 内容, float 尺寸, ImVec2 位置, ImU32 颜色, int 方向 = 0, bool 瞄边 = false, bool 背景 = false) {
    ImVec2 内容尺寸 = ImGui::CalcTextSize(内容.c_str());
    const float 基准尺寸 = 30.0f;
    const float 基准比例 = 0.5f;
       
    float 尺寸比例 = 尺寸 / 基准尺寸;
       
    float 偏移比例 = 基准比例 * 尺寸比例;
    
    if (背景) {
        绘制矩形(ImDrawList, ImVec4(位置.x - (内容尺寸.x * 0.75f) * 0.5f, 位置.y - (尺寸 + 10.0f) * 0.5f, 内容尺寸.x * 0.75f, 尺寸 + 10.0f), IM_COL32(0, 0, 0, 128), 2.0f);
    };
       
    if (瞄边) {
        for (int X = -1; X <= 1; X++) {
            for (int Y = -1; Y <= 1; Y++) {
                if (X == 0 && Y == 0) continue;
                switch (方向) {
                    case 0:
                        ImDrawList -> AddText(ImGui::GetFont(), 尺寸, ImVec2(位置.x - 内容尺寸.x * 偏移比例 + X, 位置.y - 内容尺寸.y * 偏移比例 + Y), IM_COL32_BLACK, 内容.c_str());
                        break;
                    case 1:
                        ImDrawList -> AddText(ImGui::GetFont(), 尺寸, ImVec2(位置.x + X, 位置.y - 内容尺寸.y * 偏移比例 + Y), IM_COL32_BLACK, 内容.c_str());
                        break;
                    case 2:
                        ImDrawList -> AddText(ImGui::GetFont(), 尺寸, ImVec2(位置.x - 内容尺寸.x + X, 位置.y - 内容尺寸.y * 偏移比例 + Y), IM_COL32(0, 0, 0, 200), 内容.c_str());
                        break;
                };
            };
        };
    };
    
    switch (方向) {
        case 0:
            ImDrawList -> AddText(ImGui::GetFont(), 尺寸, ImVec2(位置.x - 内容尺寸.x * 偏移比例, 位置.y - 内容尺寸.y * 偏移比例), 颜色, 内容.c_str());
            break;
        case 1:
            ImDrawList -> AddText(ImGui::GetFont(), 尺寸, ImVec2(位置.x, 位置.y - 内容尺寸.y * 偏移比例), 颜色, 内容.c_str());
            break;
        case 2:
            ImDrawList -> AddText(ImGui::GetFont(), 尺寸, ImVec2(位置.x - 内容尺寸.x, 位置.y - 内容尺寸.y * 偏移比例), 颜色, 内容.c_str());
            break;
    };
};

void 绘制血条(ImDrawList* ImDrawList, ImVec2 位置, float Health, float ImpendingDeathHealth, bool bIsAI) {
    int 总格子数 = 4;
    float 每段宽度 = 25.0f;
    float 每段高度 = 8.0f;
    float 每段间隙 = 2.0f;
    float 圆角 = 1.0f;
    float 每格血量 = 100.0f / 总格子数;
    Health = std::clamp(Health, 0.0f, 100.0f);
    
    for (int i = 0; i < 总格子数; ++i) {
        float X = 位置.x - 54.0f + i * (每段宽度 + 每段间隙);
        float Y = 位置.y - 20.0f;
        float 格子内血量 = std::clamp((Health > 0.0f ? Health : ImpendingDeathHealth) - i * 每格血量, 0.0f, 每格血量);
        float 比例 = 格子内血量 / 每格血量;
     
        if (比例 > 0.0f) {
            ImDrawList->AddRectFilled(ImVec2(X, Y), ImVec2(X + 比例 * 每段宽度, Y + 每段高度), bIsAI &&Health > 0.0f  ? IM_COL32(102, 255, 102, 255) : !bIsAI &&Health ? IM_COL32(255, 102, 102, 255) : IM_COL32(255, 255, 102, 255), 圆角);
        };
        ImDrawList->AddRect(ImVec2(X, Y), ImVec2(X + 每段宽度, Y + 每段高度), IM_COL32_BLACK, 圆角);
    };
};
