import sys

from .state_engine import analyze_plant


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


def print_state(title, state):
    print(f"\n[{title}]")
    print("环境状态：", state.env_status)
    print("叶片状态：", state.leaf_status)
    print("叶片范围：", state.leaf_scope)
    print("舒适分：", state.comfort_score)
    print("健康分：", state.health_score)
    print("心情：", state.mood)
    print("表情：", state.face)
    print("解释：", state.summary)
    if state.issues:
        print("问题：")
        for item in state.issues:
            print("-", item)
    print("建议：")
    for item in state.advice:
        print("-", item)
    if state.care_notes:
        print("长期护理：")
        for item in state.care_notes:
            print("-", item)


def main():
    cases = [
        (
            "健康状态",
            analyze_plant(
                soil_moisture=45,
                temperature=26,
                air_humidity=55,
                light=500,
                touched=True,
                leaf_status="healthy",
            ),
        ),
        (
            "缺水状态",
            analyze_plant(
                soil_moisture=18,
                temperature=31,
                air_humidity=45,
                light=650,
                leaf_status="healthy",
            ),
        ),
        (
            "疑似病叶",
            analyze_plant(
                soil_moisture=70,
                temperature=28,
                air_humidity=70,
                light=450,
                leaf_status="leaf_spot",
            ),
        ),
        (
            "大规模病变",
            analyze_plant(
                soil_moisture=45,
                temperature=26,
                air_humidity=55,
                light=500,
                leaf_status="leaf_spot",
                leaf_scope="widespread",
            ),
        ),
    ]

    print("Plant Pet AI demo")
    for title, state in cases:
        print_state(title, state)


if __name__ == "__main__":
    main()
