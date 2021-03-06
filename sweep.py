import logging
import subprocess
from pathlib import Path

import hydra  # type: ignore
from omegaconf import DictConfig

logger = logging.getLogger(__name__)

initialized = False


@hydra.main(config_path="conf/", config_name="ax")  # type: ignore
def main(cfg: DictConfig) -> float:
    field_wt_list = [f"{k[:-3]}:{v}" for k, v in cfg.items() if k.endswith("_wt")]
    field_wt = ",".join(field_wt_list)

    field_b_list = [f"{k[:-2]}:{v}" for k, v in cfg.items() if k.endswith("_b")]
    field_b = ",".join(field_b_list)
    output_path = f"k1={cfg.k1}-fieldWt={field_wt}-fieldB={field_b}.run"

    cmd = (
        f"{cfg.script_path} -index={cfg.index_path} -k1={cfg.k1} "
        f"-fieldWt={field_wt} -fieldB={field_b} "
        f"-threads={cfg.threads} {cfg.query_path}"
    )
    out = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, encoding="utf-8")
    with open(output_path, "w") as f:
        f.write(out.stdout)

    trec_cmd = f"trec_eval -m {cfg.metric} {cfg.qrels_path} {output_path}"
    proc = subprocess.run(trec_cmd, shell=True, stdout=subprocess.PIPE, text=True)
    value = proc.stdout.split()[2]
    new_path = (
        f"k1={cfg.k1}-fieldWt={field_wt}-fieldB={field_b}-{cfg.metric}={value}.run"
    )

    Path(output_path).rename(new_path)
    logger.info(new_path)
    return float(value)


if __name__ == "__main__":
    main()
