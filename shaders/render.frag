#version 330

in vec3 f_positionCameraSpace;
in vec3 f_normalCameraSpace;
in vec3 f_lightPosCameraSpace;

// Varying変数
in vec3 f_fragColor;

// ディスプレイへの出力変数
out vec4 out_color;

// マテリアルのデータ
uniform vec3 u_diffColor;
uniform vec3 u_specColor;
uniform vec3 u_ambiColor;
uniform float u_shininess;

// 選択を判定するためのID
uniform int u_cubeType;
uniform int u_cubeID;

// 出力の種類
uniform int u_outColorMode;

void main() {
    if (u_cubeType > 0) {
        // 選択のIDが0より大きければIDで描画する
        float r = u_cubeType / 255.0;
        float g = u_cubeID / 255.0;
        out_color = vec4(r, g, r, 1.0);
    } else {

        // カメラ座標系を元にした局所座標系への変換
        vec3 V = normalize(-f_positionCameraSpace);
        vec3 N = normalize(f_normalCameraSpace);
        vec3 L = normalize(f_lightPosCameraSpace - f_positionCameraSpace);
        vec3 H = normalize(V + L);

        // Blinn-Phongの反射モデル
        float ndotl = max(0.0, dot(N, L));
        float ndoth = max(0.0, dot(N, H));

        // 色モードに応じて表示する色を変更する。
        if (u_outColorMode == 0) {
            // 描画色を代入
            out_color = vec4(f_fragColor, 1.0);
        } else if (u_outColorMode == 1) {
            
            vec3 diffuse = f_fragColor * ndotl;
            vec3 specular = vec3(1.0) * pow(ndoth, u_shininess);
            vec3 ambient = 0.5 * f_fragColor;

            out_color = vec4(diffuse + specular + ambient, 1.0);

        } else {

            vec3 diffuse = u_diffColor * ndotl;
            vec3 specular = u_specColor * pow(ndoth, u_shininess);
            vec3 ambient = u_ambiColor;

            out_color = vec4(diffuse + specular + ambient, 1.0);
        }
    }
}