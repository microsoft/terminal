import yaml
import json
import sys
import os

def parse_yaml_files(tool, directory):
    json_data = {}
    json_data["name"] = f"{tool}..."
    json_data["commands"] = []

    for filename in os.listdir(directory):
        if filename.endswith(".yaml") or filename.endswith(".yml"):
            file_path = os.path.join(directory, filename)
            with open(file_path, 'r', encoding="utf-8") as file:
                try:
                    yaml_data = yaml.safe_load(file)
                    new_obj = {}
                    command = {}
                    command["input"] = yaml_data["command"]
                    command["action"] ="sendInput"

                    new_obj["command"]=command
                    new_obj["name"] = yaml_data["name"]

                    new_obj["description"] = yaml_data["description"] if "description" in yaml_data else ""
                    json_data["commands"].append(new_obj)
                except yaml.YAMLError as e:
                    print(f"Error parsing {filename}: {e}")
                    sys.exit(-1)
    return json_data

def main(directory) -> int:
    json_data = {}
    json_data["actions"] = []

    for tool_dir in os.listdir(directory):
        # print(tool_dir)
        json_data["actions"].append(parse_yaml_files(tool_dir, os.path.join(directory, tool_dir)))
    print(json.dumps(json_data, indent=4))
    return 0

if __name__ == '__main__':
    # Write this output to something like 
    # "%localappdata%\Microsoft\Windows Terminal\Fragments\warp-workflows\actions.json"
    sys.exit(main("d:\\dev\\public\\workflows\\specs"))  